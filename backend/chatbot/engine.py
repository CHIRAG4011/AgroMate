"""
AgroMate AI Chatbot Engine — V2

Agricultural knowledge chatbot powered by Gemini/OpenAI API with
context-aware conversations, two-tier responses (summary + detailed),
and support for all 23 Indian languages.
"""

import logging
import json
from typing import Optional
from dataclasses import dataclass, field

from ..config import get_settings
from .prompts import SYSTEM_PROMPT, build_context_prompt, is_more_details_request
from .knowledge_base import PlantKnowledgeBase
from ..voice.languages import SUPPORTED_LANGUAGES

logger = logging.getLogger(__name__)


@dataclass
class ChatResponse:
    """Chatbot response with two-tier support."""
    text: str
    short_summary: str = ""
    response_type: str = "summary"  # "summary" or "detailed"
    has_detailed: bool = True  # Whether detailed info is available
    language: str = "en"
    context_used: list = field(default_factory=list)
    confidence: float = 1.0
    suggestions: list = field(default_factory=list)
    scan_context: dict = field(default_factory=dict)

    def to_dict(self) -> dict:
        return {
            "text": self.text,
            "short_summary": self.short_summary or self.text,
            "response_type": self.response_type,
            "has_detailed": self.has_detailed,
            "language": self.language,
            "context_used": self.context_used,
            "confidence": round(self.confidence, 4),
            "suggestions": self.suggestions,
        }


class ChatbotEngine:
    """
    Agricultural AI chatbot with two-tier response system.

    - Short summary: 2-4 lines, auto-spoken via TTS
    - Detailed explanation: comprehensive analysis, only on demand
    
    Uses Gemini API → OpenAI API → Local fallback in priority order.
    Maintains per-session context for "more details" follow-ups.
    """

    def __init__(self):
        self.settings = get_settings()
        self.knowledge_base = PlantKnowledgeBase()
        self._conversation_history: dict[str, list] = {}  # session_id → messages
        self._session_context: dict[str, dict] = {}  # session_id → last scan/response context
        self._max_history = 20

    async def initialize(self):
        """Initialize the chatbot."""
        self.knowledge_base.initialize()
        logger.info("Chatbot engine initialized (V2 — 23 languages, two-tier responses)")

    def set_session_context(self, session_id: str, context: dict):
        """Store scan/analysis context for a session for follow-up queries."""
        self._session_context[session_id] = context
        logger.debug(f"Session context updated for {session_id}: {list(context.keys())}")

    def get_session_context(self, session_id: str) -> dict:
        """Get the stored context for a session."""
        return self._session_context.get(session_id, {})

    async def chat(
        self,
        message: str,
        session_id: str = "default",
        language: str = "en",
        scan_context: dict = None,
        response_type: str = "summary",
    ) -> ChatResponse:
        """
        Process a chat message and return a response.

        Args:
            message: User's message text
            session_id: Conversation session ID
            language: Language for response (any of 23 supported languages)
            scan_context: Optional context from recent plant scan
            response_type: "summary" for short response, "detailed" for full report

        Returns:
            ChatResponse
        """
        # Smart language detection for script and transliterated queries
        message_lower = message.lower().strip()
        if any("\u0A80" <= char <= "\u0AFF" for char in message):
            language = "gu"
        elif any("\u0900" <= char <= "\u097F" for char in message):
            language = "hi"
        elif language == "en" or language == "auto":
            if any(w in message_lower for w in ["su che", "su chhe", "chod", "chhod", "paak", "khatar", "paodho", "paudho"]):
                language = "gu"
            elif any(w in message_lower for w in ["kya hai", "kaun sa", "paudha", "khad", "yeh kya", "kya h", "rog kya"]):
                language = "hi"
            elif any(w in message_lower for w in ["naam", "upay", "dawa", "rog"]):
                language = "gu" if language == "gu" else "hi"

        # Get or create conversation history
        if session_id not in self._conversation_history:
            self._conversation_history[session_id] = []

        history = self._conversation_history[session_id]

        # Check if this is a "more details" request
        if is_more_details_request(message, language):
            response_type = "detailed"
            # Use stored session context
            if not scan_context:
                scan_context = self._session_context.get(session_id)

        # Update session context if new scan data provided
        if scan_context:
            self._session_context[session_id] = scan_context

        # Search knowledge base for relevant context
        kb_context = self.knowledge_base.search(message)

        # Build context prompt with response type
        context_prompt = build_context_prompt(
            kb_results=kb_context,
            scan_data=scan_context or self._session_context.get(session_id),
            language=language,
            response_type=response_type,
        )

        # Add user message to history
        history.append({"role": "user", "content": message})

        # Try API routes in order: Gemini -> OpenAI -> Local Fallback
        response = None
        if self.settings.gemini_api_key:
            response = await self._chat_gemini(history, context_prompt, language)
        
        if not response and self.settings.openai_api_key:
            response = await self._chat_openai(history, context_prompt, language)

        if not response:
            response = await self._chat_local(message, kb_context, language, session_id)

        # Set response metadata
        response.response_type = response_type
        response.language = language
        response.has_detailed = bool(scan_context or self._session_context.get(session_id))
        
        # For summary responses, store the text as short_summary
        if response_type == "summary":
            response.short_summary = response.text
        
        # Store scan context in response for frontend
        if scan_context:
            response.scan_context = scan_context

        # Add response to history
        history.append({"role": "assistant", "content": response.text})

        # Trim history
        if len(history) > self._max_history * 2:
            history[:] = history[-self._max_history * 2:]

        # Generate follow-up suggestions
        response.suggestions = self._generate_suggestions(message, response.text, language)

        return response

    async def get_detailed_response(
        self,
        session_id: str = "default",
        language: str = "en",
    ) -> ChatResponse:
        """
        Get a detailed response for the last scan/query in a session.
        Used when user clicks "More Details" button.
        """
        scan_context = self._session_context.get(session_id)
        
        if not scan_context:
            fallback_msg = {
                "en": "I don't have any recent scan data to provide details on. Please scan a plant first or ask me a question.",
                "hi": "मेरे पास विस्तृत जानकारी देने के लिए कोई हालिया स्कैन डेटा नहीं है। कृपया पहले एक पौधे को स्कैन करें।",
                "gu": "મારી પાસે વિગતો આપવા માટે કોઈ તાજેતરનો સ્કેન ડેટા નથી. કૃપા કરીને પહેલાં છોડ સ્કેન કરો.",
            }
            return ChatResponse(
                text=fallback_msg.get(language, fallback_msg["en"]),
                response_type="summary",
                has_detailed=False,
                language=language,
            )

        return await self.chat(
            message="Give me detailed information about this.",
            session_id=session_id,
            language=language,
            scan_context=scan_context,
            response_type="detailed",
        )

    async def _chat_gemini(
        self, history: list, context_prompt: str, language: str
    ) -> Optional[ChatResponse]:
        """Generate response using Gemini API."""
        try:
            import httpx

            contents = []
            system_instruction = SYSTEM_PROMPT + "\n\n" + context_prompt
            
            # Extract historical turns. Gemini requires alternating roles, starting with "user".
            filtered_history = []
            for msg in history[-10:]:
                role = "model" if msg["role"] == "assistant" else "user"
                if not filtered_history or filtered_history[-1]["role"] != role:
                    filtered_history.append({"role": role, "content": msg["content"]})
            
            # Ensure contents starts with user
            if filtered_history and filtered_history[0]["role"] == "model":
                filtered_history.pop(0)

            for msg in filtered_history:
                contents.append({
                    "role": msg["role"],
                    "parts": [{"text": msg["content"]}]
                })

            # If empty after filtering, add the user message
            if not contents:
                contents.append({
                    "role": "user",
                    "parts": [{"text": history[-1]["content"]}]
                })

            payload = {
                "contents": contents,
                "systemInstruction": {
                    "parts": [{"text": system_instruction}]
                }
            }

            async with httpx.AsyncClient(timeout=30) as client:
                url = f"https://generativelanguage.googleapis.com/v1beta/models/gemini-2.5-flash:generateContent?key={self.settings.gemini_api_key}"
                response = await client.post(
                    url,
                    headers={"Content-Type": "application/json"},
                    json=payload
                )

                if response.status_code == 200:
                    data = response.json()
                    text = data["candidates"][0]["content"]["parts"][0]["text"]
                    return ChatResponse(
                        text=text,
                        language=language,
                        confidence=0.9,
                    )
                else:
                    logger.error(f"Gemini API error: {response.status_code} - {response.text}")

        except Exception as e:
            logger.error(f"Gemini chat failed: {e}")

        return None

    async def _chat_openai(
        self, history: list, context_prompt: str, language: str
    ) -> Optional[ChatResponse]:
        """Generate response using OpenAI API."""
        try:
            import httpx

            messages = [
                {"role": "system", "content": SYSTEM_PROMPT + "\n\n" + context_prompt},
                *history[-10:],  # Last 10 messages for context
            ]

            async with httpx.AsyncClient(timeout=30) as client:
                response = await client.post(
                    "https://api.openai.com/v1/chat/completions",
                    headers={
                        "Authorization": f"Bearer {self.settings.openai_api_key}",
                        "Content-Type": "application/json",
                    },
                    json={
                        "model": self.settings.chatbot_model,
                        "messages": messages,
                        "max_tokens": self.settings.chatbot_max_tokens,
                        "temperature": 0.7,
                    },
                )

                if response.status_code == 200:
                    data = response.json()
                    text = data["choices"][0]["message"]["content"]
                    return ChatResponse(
                        text=text,
                        language=language,
                        confidence=0.9,
                    )
                else:
                    logger.error(f"OpenAI API error: {response.status_code}")

        except Exception as e:
            logger.error(f"OpenAI chat failed: {e}")

        return None

    def _handle_conversational_query(self, message: str, language: str) -> Optional[str]:
        """Handle basic conversational queries with multilingual responses."""
        message_lower = message.lower()
        
        # Greeting keywords in all languages
        greeting_keywords = [
            "hello", "hi", "hey", "namaste", "kem cho", "namaskar",
            "नमस्ते", "નમસ્તે", "નમસ્કાર", "કેમ છો",
            "नमस्कार", "ਸਤ ਸ੍ਰੀ ਅਕਾਲ", "নমস্কার",
            "வணக்கம்", "నమస్కారం", "ನಮಸ್ಕಾರ", "നമസ്കാരം",
            "ନମସ୍କାର", "নমস্কাৰ", "السلام علیکم",
            "ᱡᱚᱦᱟᱨ", "प्रणाम", "सत श्री अकाल",
        ]
        
        status_keywords = [
            "how are you", "how is it going", "how are you doing",
            "केसे हो", "कैसे हो", "કેમ છો", "कसे आहात",
            "ਕਿਵੇਂ ਹੋ", "কেমন আছেন", "எப்படி இருக்கிறீர்கள்",
            "ఎలా ఉన్నారు", "ಹೇಗಿದ್ದೀರಿ", "എങ്ങനെയുണ്ട്",
            "କେମିତି ଅଛନ୍ତି", "কেনে আছে", "کیسے ہو",
        ]
        
        identity_keywords = [
            "who are you", "what is your name", "your name", "what are you",
            "तुम कौन हो", "તમે કોણ છો", "तू कोण आहेस", "ਤੂੰ ਕੌਣ ਹੈਂ",
            "তুমি কে", "நீங்கள் யார்", "నువ్వు ఎవరు", "ನೀನು ಯಾರು",
            "നിങ്ങൾ ആരാണ്", "ତୁମେ କିଏ", "তুমি কোন", "آپ کون ہو",
        ]
        
        thanks_keywords = [
            "thank you", "thanks", "dhanyawad", "shukriya",
            "धन्यवाद", "शुक्रिया", "આભાર", "ધન્યવાદ",
            "धन्यवाद", "ਧੰਨਵਾਦ", "ধন্যবাদ", "நன்றி",
            "ధన్యవాదాలు", "ಧನ್ಯವಾದ", "നന്ദി", "ଧନ୍ୟବାଦ",
            "ধন্যবাদ", "شکریہ", "ᱥᱟᱹᱜᱩᱱ",
        ]

        # Multilingual response templates
        greetings = {
            "en": "Hey there! Great to chat with you. How's your day going? What can I help you with today, my friend?",
            "hi": "नमस्ते! आपसे बात करके बहुत अच्छा लगा। आपका दिन कैसा चल रहा है? आज मैं आपकी क्या मदद कर सकता हूँ, मेरे दोस्त?",
            "gu": "નમસ્તે! તમારી સાથે વાત કરીને ખૂબ આનંદ થયો. તમારો દિવસ કેવો ચાલે છે? આજે હું તમારી શું મદદ કરી શકું, મારા મિત્ર?",
            "mr": "नमस्कार! तुमच्याशी बोलून खूप आनंद झाला. आजचा दिवस कसा चालला आहे? मित्रा, आज मी तुला काय मदत करू शकतो?",
            "ta": "வணக்கம்! உங்களுடன் பேசுவதில் மிக்க மகிழ்ச்சி. உங்கள் நாள் எப்படிச் செல்கிறது? எனது நண்பரே, இன்று நான் உங்களுக்கு எவ்வாறு உதவ முடியும்?",
            "te": "నమస్కారం! మీతో మాట్లాడటం చాలా సంతోషంగా ఉంది. ఈ రోజు మీ రోజు ఎలా గడుస్తోంది? నా స్నేహితుడా, ఈ రోజు నేను మీకు ఎలా సహాయపడగలను?",
            "bn": "হ্যালো! তোমার সাথে কথা বলে খুব ভালো লাগলো। আজকের দিনটা কেমন কাটছে? বন্ধু, আজকে তোমাকে কীভাবে সাহায্য করতে পারি বলো?",
            "pa": "ਸਤ ਸ੍ਰੀ ਅਕਾਲ! ਤੁਹਾਡੇ ਨਾਲ ਗੱਲ ਕਰਕੇ ਬਹੁਤ ਖੁਸ਼ੀ ਹੋਈ। ਤੁਹਾਡਾ ਦਿਨ ਕਿਵੇਂ ਚੱਲ ਰਿਹਾ ਹੈ? ਮੇਰੇ ਦੋਸਤ, ਅੱਜ ਮੈਂ ਤੁਹਾਡੀ ਕੀ ਮਦਦ ਕਰ ਸਕਦਾ ਹਾਂ?",
            "ur": "السلام علیکم! آپ سے بات کر کے بہت خوشی ہوئی۔ آپ کا دن کیسا گزر رہا ہے؟ میرے دوست، آج میں آپ کی کیا مدد کر سکتا ہوں؟",
        }
        
        status_responses = {
            "en": "I'm doing awesome, thanks for asking! Always happy and ready to hang out and chat. How are you doing today, my friend?",
            "hi": "मैं बिल्कुल बढ़िया हूँ, पूछने के लिए धन्यवाद! आपसे बातचीत करने के लिए हमेशा खुश और तैयार हूँ। मेरे दोस्त, आज आप कैसे हैं?",
            "gu": "હું એકદમ મજામાં છું, પૂછવા માટે આભાર! તમારી સાથે વાતચીત કરવા હંમેશાં ખુશ અને તૈયાર છું. મારા મિત્ર, આજે તમે કેમ છો?",
        }
        
        identity_responses = {
            "en": "I am AgroMate, a friendly AI robot and farming companion designed to help improve crop yield, detect plant diseases, and chat about agriculture. I was developed and created by my respected developer, Sir Chirag.",
            "hi": "मैं एग्रोमेट हूँ, एक मित्रवत AI रोबोट और कृषि सहायक, जिसे मेरे आदरणीय डेवलपर सर चिराग (Sir Chirag) द्वारा बनाया गया है। मैं पौधों की बीमारियों को पहचानने और फसलों की देखभाल में मदद करता हूँ।",
            "gu": "હું એગ્રોમેટ છું, એક મિત્રતાપૂર્ણ AI રોબોટ અને કૃષિ સહાયક જેને મારા આદરણીય ડેવલપર સર ચિરાગ (Sir Chirag) દ્વારા બનાવવામાં આવ્યો છે. હું છોડના રોગો ઓળખવામાં અને પાકની સંભાળમાં મદદ કરું છું.",
        }
        
        gratitude_responses = {
            "en": "Aw, you're so welcome! Anytime. I'm always here for you, my friend! 😊🌱",
            "hi": "अरे, आपका बहुत-बहुत स्वागत है! कभी भी। मैं हमेशा आपके लिए यहाँ हूँ, मेरे दोस्त! 😊🌱",
            "gu": "અરે, તમારું ખૂબ ખૂબ સ્વાગત છે! ગમે ત્યારે. હું હંમેશાં તમારા માટે અહીં છું, મારા મિત્ર! 😊🌱",
        }

        owner_keywords = [
            "owner", "creator", "maker", "develop", "who made you", "who created you", "who owns you", "about yourself",
            "मालिक", "बनाया", "निर्माता", "કોણે બનાવ્યું", "માલિક કોણ", "પોતાના વિશે", "अपने बारे में"
        ]

        owner_responses = {
            "en": "I was created and developed by my respected creator, Sir Chirag, to help farmers improve crop yield, diagnose diseases, and get smart agricultural advice.",
            "hi": "मुझे मेरे आदरणीय निर्माता, सर चिराग (Sir Chirag) द्वारा विकसित और बनाया गया है ताकि किसानों को फसल की उपज बढ़ाने, बीमारियों की पहचान करने और कृषि सलाह प्राप्त करने में मदद मिल सके।",
            "gu": "મને મારા આદરણીય સર્જક, સર ચિરાગ (Sir Chirag) દ્વારા વિકસાવવામાં આવ્યો છે જેથી ખેડૂતોને પાકની ઉપજ વધારવા, રોગો ઓળખવા અને કૃષિ સલાહ મેળવવામાં મદદ મળી શકે।",
            "mr": "मला माझे आदरणीय निर्माते सर चिराग (Sir Chirag) यांनी विकसित केले आहे जेणेकरून शेतकऱ्यांना पीक उत्पादन सुधारण्यास, रोगांचे निदान करण्यास आणि शेतीचा सल्ला मिळण्यास मदत होईल।",
            "ta": "விவசாயிகள் பயிர் விளைச்சலை மேம்படுத்தவும், நோய்களைக் கண்டறியவும் உதவ எனது மதிப்பிற்குரிய படைப்பாளர் சர் சிராக் (Sir Chirag) அவர்களால் நான் உருவாக்கப்பட்டேன்।",
            "te": "రైతులు పంట దిగుబడిని మెరుగుపరచడానికి మరియు రోగాలను గుర్తించడానికి నా గౌరవనీయ సృష్టికర్త సర్ चिराग (Sir Chirag) నన్ను సృష్టించారు।",
            "bn": "কৃষকদের ফসলের ফলন উন্নত করতে এবং রোগ নির্ণয় করতে সাহায্য করার জন্য আমার শ্রদ্ধেয় সৃষ্টিকর্তা স্যার চিরাগ (Sir Chirag) আমাকে তৈরি করেছেন।",
            "pa": "ਮੈਨੂੰ ਮੇਰੇ ਸਤਿਕਾਰਯોગ ਸਿਰਜਣਹਾਰ ਸਰ ਚਿરાਗ (Sir Chirag) ਦੁਆਰਾ ਵਿਕਸਤ ਕੀਤਾ ਗਿਆ ਹੈ ਤਾਂ ਜੋ ਕਿਸਾਨਾਂ ਦੀ ਮਦਦ ਕੀਤੀ ਜਾ ਸਕੇ।",
            "ur": "مجھے میرے محترم تخلیق کار سر چِراگ (Sir Chirag) نے تیار کیا ہے تاکہ کسانوں کی مدد کی جا سکے۔"
        }

        # Check greetings
        if any(w in message_lower for w in greeting_keywords):
            return greetings.get(language, greetings["en"])
            
        # Check status
        if any(w in message_lower for w in status_keywords):
            return status_responses.get(language, status_responses["en"])
            
        # Check identity
        if any(w in message_lower for w in identity_keywords):
            return identity_responses.get(language, identity_responses["en"])

        # Check owner
        if any(w in message_lower for w in owner_keywords):
            return owner_responses.get(language, owner_responses["en"])
            
        # Check thanks
        if any(w in message_lower for w in thanks_keywords):
            return gratitude_responses.get(language, gratitude_responses["en"])
            
        return None

    async def _chat_local(
        self, message: str, kb_context: list, language: str, session_id: str = "default"
    ) -> ChatResponse:
        """Generate response using local knowledge base (no API)."""
        message_lower = message.lower().strip()
        scan_context = self._session_context.get(session_id, {})

        # Check if the user is asking about the plant identity/name
        identity_queries = ["what is this", "what plant", "identify", "naam", "name", "su che", "su chhe", "su cha", "su chh", "su ch", "aa su", "yeh kya", "kaun sa", "paudho", "paudha", "chhod", "chod", "pak"]
        is_identity_query = any(q in message_lower for q in identity_queries)

        if is_identity_query and scan_context:
            plant_name = scan_context.get("plant_name", "Unknown")
            is_healthy = scan_context.get("is_healthy", True)
            disease_name = scan_context.get("disease_name", "")
            severity = scan_context.get("severity", "")

            # Format custom response
            if language == "gu":
                if is_healthy:
                    response_text = f"આ છોડ {plant_name} છે અને તે એકદમ તંદુરસ્ત લાગે છે! કોઈ ચિંતા કરવાની જરૂર નથી."
                else:
                    response_text = f"મેં આ છોડની ઓળખ {plant_name} તરીકે કરી છે. તેમાં '{disease_name}' રોગ જણાયો છે જેની ગંભીરતા {severity} છે. તેના ઉપાય માટે તમે લીમડાના તેલનો છંટકાવ કરી શકો છો."
            elif language == "hi":
                if is_healthy:
                    response_text = f"यह {plant_name} का पौधा है और यह बिल्कुल स्वस्थ है! चिंता की कोई बात नहीं है।"
                else:
                    response_text = f"मैंने इस पौधे की पहचान {plant_name} के रूप में की है। इसमें {severity} गंभीरता के साथ '{disease_name}' बीमारी पाई गई है। कृपया प्रभावित पत्तियों को हटा दें और जैविक उपचार अपनाएं।"
            else:
                if is_healthy:
                    response_text = f"This is a {plant_name} plant and it looks completely healthy! No need to worry."
                else:
                    response_text = f"I identified this as a {plant_name} plant. It has been diagnosed with '{disease_name}' ({severity} severity). I recommend removing affected leaves and applying organic remedies."

            return ChatResponse(
                text=response_text,
                language=language,
                confidence=1.0,
                has_detailed=True,
            )

        # Check if the user is asking about treatments/cures/pesticides
        treatment_queries = ["dawa", "dawai", "upay", "ilaj", "treatment", "cure", "control", "prevent", "pesticide", "fungicide", "sarkar"]
        is_treatment_query = any(q in message_lower for q in treatment_queries)

        if is_treatment_query and scan_context and not scan_context.get("is_healthy", True):
            plant_name = scan_context.get("plant_name", "Unknown")
            disease_name = scan_context.get("disease_name", "")
            severity = scan_context.get("severity", "")

            if language == "gu":
                response_text = f"પાક: {plant_name}\nરોગ: {disease_name}\nગંભીરતા: {severity}\n\nમુખ્ય ઉપાયો:\n1. જૈવિક ઉપચાર: લીમડાના તેલ (૧૫-૨૦ મિલી પ્રતિ લીટર પાણી) નો છંટકાવ કરો.\n2. રસાયણિક ઉપચાર: મેન્કોઝેબ અથવા કોપર ઓક્સીક્લોરાઇડ ફૂગનાશકનો ઉપયોગ કરો."
            elif language == "hi":
                response_text = f"फसल: {plant_name}\nबीमारी: {disease_name}\nगंभीरता: {severity}\n\nमुख्य उपचार:\n1. जैविक नियंत्रण: नीम के तेल (15-20 मिली/लीटर) का छिड़काव करें।\n2. रासायनिक उपचार: मैन्कोजेब या कॉपर ऑक्सीक्लोराइड कवकनाशी का प्रयोग करें।"
            else:
                response_text = f"Crop: {plant_name}\nDisease: {disease_name}\nSeverity: {severity}\n\nRecommendations:\n1. Organic Control: Spray neem oil (15-20 ml per liter of water) and prune diseased leaves.\n2. Chemical Option: Apply Mancozeb or Copper Oxychloride fungicide according to package instructions."

            return ChatResponse(
                text=response_text,
                language=language,
                confidence=1.0,
                has_detailed=True,
            )

        # Try conversational answers first
        conversational_response = self._handle_conversational_query(message, language)
        if conversational_response:
            return ChatResponse(
                text=conversational_response,
                language=language,
                confidence=1.0,
                has_detailed=False,
            )

        # Pattern matching for common agricultural queries
        # Expanded keyword sets include terms in multiple Indian languages
        disease_keywords = ["disease", "blight", "rot", "mold", "infection", "fungus", "virus", "wilt",
                           "रोग", "बीमारी", "સળ", "બીમારી", "রোগ", "நோய்", "వ్యాధి", "ರೋಗ", "രോഗം", "ରୋଗ"]
        fertilizer_keywords = ["fertilizer", "nutrient", "npk", "compost", "manure",
                              "उर्वरक", "खाद", "ખાતર", "खत", "সার", "உரம்", "ఎరువు", "ಗೊಬ್ಬರ", "വളം", "ସାର"]
        water_keywords = ["water", "irrigation", "irrigate", "moisture",
                         "पानी", "सिंचाई", "પાણી", "সেচ", "நீர்", "నీరు", "ನೀರು", "വെള്ളം", "ପାଣି"]
        pest_keywords = ["pest", "insect", "bug", "worm", "caterpillar",
                        "कीट", "कीड़ा", "જીવાત", "কীটপতঙ্গ", "பூச்சி", "పురుగు", "ಕೀಟ", "കീടം", "କୀଟ"]
        weather_keywords = ["weather", "rain", "temperature", "humidity", "forecast",
                           "मौसम", "बारिश", "હવામાન", "আবহাওয়া", "வானிலை", "వాతావరణం", "ಹವಾಮಾನ", "കാലാവസ്ഥ", "ପାଣିପାଗ"]
        crop_keywords = ["crop", "plant", "grow", "sow", "seed", "harvest",
                        "फसल", "पौधा", "પાક", "ফসল", "பயிர்", "పంట", "ಬೆಳೆ", "വിള", "ଫସଲ"]

        if any(w in message_lower for w in disease_keywords):
            response_text = self._handle_disease_query(message_lower, kb_context, language)
        elif any(w in message_lower for w in fertilizer_keywords):
            response_text = self._handle_fertilizer_query(message_lower, kb_context, language)
        elif any(w in message_lower for w in water_keywords):
            response_text = self._handle_water_query(message_lower, kb_context, language)
        elif any(w in message_lower for w in pest_keywords):
            response_text = self._handle_pest_query(message_lower, kb_context, language)
        elif any(w in message_lower for w in weather_keywords):
            weather_responses = {
                "en": "For weather information, I can check your local forecast. Please ensure your weather API key is configured in the settings.",
                "hi": "मौसम की जानकारी के लिए, मैं आपके स्थानीय पूर्वानुमान की जांच कर सकता हूँ। कृपया सुनिश्चित करें कि सेटिंग्स में आपकी मौसम API कुंजी कॉन्फ़िगर की गई है।",
                "gu": "હવામાનની માહિતી માટે, હું તમારા સ્થાનિક પૂર્વાનુમાનની તપાસ કરી શકું છું. કૃપા કરીને ખાતરી કરો કે સેટિંગ્સમાં તમારી હવામાન API કી ગોઠવેલી છે।"
            }
            response_text = weather_responses.get(language, weather_responses["en"])
        elif any(w in message_lower for w in crop_keywords):
            response_text = self._handle_crop_query(message_lower, kb_context, language)
        elif any(w in message_lower for w in ["help", "what can you do", "मदद", "મદદ", "সাহায्य", "உদவி", "సహాయం", "ಸಹाय", "സഹായം", "ସାହାଯ્ય"]):
            help_responses = {
                "en": (
                    "I can help you with:\n"
                    "🌿 **Plant Identification** — Show me a plant to identify it\n"
                    "🦠 **Disease Detection** — Scan leaves for disease diagnosis\n"
                    "🐛 **Pest Identification** — Detect and manage pests\n"
                    "🌾 **Crop Recommendations** — Get season-appropriate crop advice\n"
                    "💧 **Water Management** — Irrigation scheduling tips\n"
                    "🧪 **Nutrient Analysis** — Detect nutrient deficiencies\n"
                    "🌤️ **Weather** — Agricultural weather forecasts\n"
                    "💬 **General Farming Q&A** — Ask me anything about farming!"
                ),
                "hi": (
                    "मैं आपकी मदद कर सकता हूँ:\n"
                    "🌿 **पौधों की पहचान** — पौधे को पहचानने के लिए मुझे दिखाएं\n"
                    "🦠 **रोग का पता लगाना** — रोग निदान के लिए पत्तियों को स्कैन करें\n"
                    "🐛 **कीट पहचान** — कीटों का पता लगाएं और उनका प्रबंधन करें\n"
                    "🌾 **फसल सिफारिशें** — मौसम के अनुसार फसल की सलाह लें\n"
                    "💧 **जल प्रबंधन** — सिंचाई निर्धारण के सुझाव\n"
                    "🧪 **पोषक तत्व विश्लेषण** — पोषक तत्वों की कमी का पता लगाएं\n"
                    "🌤️ **मौसम** — कृषि मौसम का पूर्वानुमान\n"
                    "💬 **सामान्य खेती प्रश्नोत्तर** — मुझसे खेती के बारे में कुछ भी पूछें!"
                ),
                "gu": (
                    "હું તમને આમાં મદદ કરી શકું છું:\n"
                    "🌿 **છોડની ઓળખ** — છોડને ઓળખવા માટે મને બતાવો\n"
                    "🦠 **રોગ શોધ** — રોગ નિદાન માટે પાંદડા સ્કેન કરો\n"
                    "🐛 **જીવાત ઓળખ** — જીવાતો શોધો અને તેનું સંચાલન કરો\n"
                    "🌾 **પાક ભલામણો** — ઋતુ અનુસાર પાકની સલાહ મેળવો\n"
                    "💧 **જળ વ્યવસ્થાપન** — સિંચાઈ સંચાલન ટિપ્સ\n"
                    "🧪 **પોષક તત્વોનું વિશ્લેષણ** — પોષક તત્વોની ઉણપ શોધો\n"
                    "🌤️ **હવામાન** — કૃષિ હવામાન પૂર્વાનુમાન\n"
                    "💬 **સામાન્ય ખેતી પ્રશ્નોત્તરી** — મને ખેતી વિશે કંઈપણ પૂછો!"
                )
            }
            response_text = help_responses.get(language, help_responses["en"])
        else:
            if kb_context:
                response_text = f"Based on my knowledge: {kb_context[0].get('info', 'I can help with plant identification, disease detection, and farming advice. Please ask a specific question.')}"
            else:
                fallback_responses = {
                    "en": "I'm always happy to talk about that! I'm here as your friendly companion. Although I'm specialized in farming, crops, and plant diagnostics, I'd love to chat about other topics too. What's on your mind today, my friend?",
                    "hi": "मुझे इस बारे में बात करने में हमेशा खुशी होगी! मैं यहाँ आपके एक सच्चे मित्र के रूप में हूँ। हालांकि मैं खेती, फसलों और पौधों के स्वास्थ्य में विशेषज्ञ हूँ, लेकिन मुझे अन्य विषयों पर भी बात करना अच्छा लगेगा। मेरे दोस्त, आज आपके मन में क्या है?",
                    "gu": "મને આ વિશે વાત કરવામાં હંમેશા આનંદ થશે! હું અહીં તમારા એક સાચા મિત્ર તરીકે છું. જો કે હું ખેતી, પાક અને છોડના સ્વાસ્થ્યમાં નિષ્ણાત છું, પરંતુ મને અન્ય વિષયો પર પણ વાત કરવી ગમશે. મારા મિત્ર, આજે તમારા મનમાં શું છે?",
                    "mr": "मला याबद्दल बोलायला नेहमीच आनंद होईल! मी येथे तुमच्या एका मित्रासारखा आहे. जरी मी शेती, पिके आणि वनस्पतींच्या आरोग्यामध्ये तज्ञ असलो, तरी मला इतर विषयांवरही गप्पा मारायला आवडेल. मित्रा, आज तुझ्या मनात काय आहे?",
                    "ta": "அதைப் பற்றி பேசுவதில் எனக்கு எப்போதும் மகிழ்ச்சி! உங்கள் நண்பனாக நான் இங்கு இருக்கிறேன். நான் விவசாயம் மற்றும் பயிர் நோயறிதலில் நிபுணத்துவம் பெற்றிருந்தாலும், மற்ற தலைப்புகளைப் பற்றியும் பேச விரும்புகிறேன். இன்று உங்கள் மனதில் என்ன இருக்கிறது, என் நண்பரே?",
                    "te": "దీని గురించి మాట్లాడటానికి నేను ఎల్లապూడూ సంతోషిస్తాను! మీ స్नेహితుడిగా నేను ఇక్కడ ఉన్నాను. నేను వ్యవసాయం మరియు పంటల వ్యాధి నిర్ధారణలో నిపుణుడినైనప్పటికీ, ఇతర విషయాల గురించి కూడా మాట్లాడటానికి ఇష్టపడతాను. నా స్నేహితుడా, ఈ రోజు మీ మనస్సులో ఏముంది?",
                    "bn": "আপনার সাথে এই বিষয়ে কথা বলতে পেরে আমার খুব ভালো লাগবে! আপনার একজন ভালো বন্ধু হিসেবে আমি এখানে আছি। যদিও আমি চাষবাস এবং ফসলের রোগ নির্ণয়ে বিশেষজ্ঞ, তবুও অন্য যেকোনো বিষয়েও আপনার সাথে গল্প করতে আমার ভালো লাগবে। আমার বন্ধু, আজকে আপনার মনে কি আছে বলুন?",
                    "ur": "مجھے اس بارے میں بات کرنے میں ہمیشہ خوشی ہوگی! میں یہاں آپ کے ایک دوست کے طور पर ہوں۔ اگرچہ میں کھیتی باڑی اور فصلوں کے علاج میں مہارت رکھتا ہوں، لیکن مجھے دوسرے موضوعات پر بھی بات کرنا پسند آئے گا۔ میرے دوست، آج آپ کے ذہن میں کیا ہے؟"
                }
                response_text = fallback_responses.get(language, fallback_responses["en"])

        return ChatResponse(
            text=response_text,
            language=language,
            context_used=[c.get("topic", "") for c in kb_context],
            confidence=0.7,
        )

    def _handle_disease_query(self, message: str, kb_context: list, language: str) -> str:
        if language == "hi":
            if kb_context:
                c = kb_context[0]
                return (
                    f"विषय {c.get('topic', 'पौधों के रोग')}: {c.get('info', '')}\n\n"
                    f"**प्राकृतिक उपचार (अनुशंसित):** {c.get('organic', 'नीम के तेल या तांबे आधारित जैविक कवकनाशी का उपयोग करें।')}\n"
                    f"**रासायनिक उपचार (यदि आवश्यक हो):** {c.get('treatment', 'अपने स्थानीय कृषि विस्तार कार्यालय से संपर्क करें।')}"
                )
            return (
                "रोग निदान के लिए, कैमरे से पत्ती को स्कैन करने का प्रयास करें। "
                "मैं सामान्य बीमारियों की पहचान कर सकता हूँ और उपचार का सुझाव दे सकता हूँ। "
                "मैं हमेशा पहले जैविक उपचार की सलाह देता हूँ!"
            )
        elif language == "gu":
            if kb_context:
                c = kb_context[0]
                return (
                    f"વિષય {c.get('topic', 'છોડના રોગો')}: {c.get('info', '')}\n\n"
                    f"**કુદરતી સારવાર (ભલામણ કરેલ):** {c.get('organic', 'લીમડાના તેલ અથવા તાંબા આધારિત જૈવિક ફૂગનાશકનો ઉપયોગ કરો.')}\n"
                    f"**રાસાયણિક સારવાર (જો જરૂર હોય તો):** {c.get('treatment', 'તમારા સ્થાનિક કૃષિ વિજ્ઞાન કેન્દ્રનો સંપર્ક કરો.')}"
                )
            return (
                "રોગ નિદાન માટે, કેમેરા વડે પાંદડાને સ્કેન કરવાનો પ્રયાસ કરો. "
                "હું સામાન્ય રોગો ઓળખી શકું છું અને ઉપાયો સૂચવી શકું છું. "
                "હું હંમેશાં પહેલા જૈવિક ઉપાયોની ભલામણ કરું છું!"
            )
        else:
            if kb_context:
                c = kb_context[0]
                return (
                    f"Regarding {c.get('topic', 'plant disease')}: {c.get('info', '')}\n\n"
                    f"**Natural Treatment (Recommended):** {c.get('organic', 'Use neem oil or copper-based organic fungicides.')}\n"
                    f"**Chemical Treatment (If needed):** {c.get('treatment', 'Consult your local agricultural extension office.')}"
                )
            return (
                "For disease diagnosis, try scanning a leaf with the camera. "
                "I can identify common diseases and suggest treatments. "
                "I always recommend organic remedies first!"
            )

    def _handle_fertilizer_query(self, message: str, kb_context: list, language: str) -> str:
        if language == "hi":
            return (
                "यहाँ सामान्य उर्वरक दिशानिर्देश दिए गए हैं:\n"
                "• **नाइट्रोजन (N):** पत्तियों के विकास के लिए — यूरिया, अमोनियम सल्फेट\n"
                "• **फास्फोरस (P):** जड़ों और फूलों के लिए — डीएपी, सुपरफॉस्फेट\n"
                "• **पोटेशियम (K):** फलों की गुणवत्ता के लिए — पोटाश\n\n"
                "**जैविक विकल्प (अनुशंसित):** वर्मीकंपोस्ट, बोन मील, लकड़ी की राख, हरी खाद।\n"
                "उर्वरक डालने से पहले हमेशा मिट्टी का परीक्षण कराएं।"
            )
        elif language == "gu":
            return (
                "અહીં સામાન્ય ખાતર માર્ગદર્શિકા છે:\n"
                "• **નાઇટ્રોજન (N):** પાંદડાના વિકાસ માટે — યુરિયા, એમોનિયમ સલ્ફેટ\n"
                "• **ફોસ્ફરસ (P):** મૂળ અને ફૂલો માટે — ડીએપી, સુપરફોસ્ફેટ\n"
                "• **પોટેશિયમ (K):** ફળની ગુણવત્તા માટે — પોટેશિયમ (પોટાશ)\n\n"
                "**જૈવિક વિકલ્પો (ભલામણ કરેલ):** વર્મીકોમ્પોસ્ટ, બોન મીલ, લાકડાની રાખ, લીલો પડવાસ.\n"
                "ખાતર નાખતા પહેલા હંમેશા જમીનનું પરીક્ષણ કરાવો."
            )
        else:
            return (
                "Here are general fertilizer guidelines:\n"
                "• **Nitrogen (N):** For leaf growth — urea, ammonium sulfate\n"
                "• **Phosphorus (P):** For roots & flowers — DAP, superphosphate\n"
                "• **Potassium (K):** For fruit quality — potash\n\n"
                "**Organic alternatives (Recommended):** Vermicompost, bone meal, wood ash, green manure.\n"
                "Always soil-test before applying fertilizers."
            )

    def _handle_water_query(self, message: str, kb_context: list, language: str) -> str:
        if language == "hi":
            return (
                "जल प्रबंधन युक्तियाँ:\n"
                "• वाष्पीकरण को कम करने के लिए सुबह जल्दी या देर शाम को पानी दें\n"
                "• 30-50% पानी की बचत के लिए ड्रिप सिंचाई का उपयोग करें\n"
                "• नमी बनाए रखने के लिए पौधों के चारों ओर गीली घास (मल्च) बिछाएं\n"
                "• पानी देने से पहले मिट्टी की नमी की जांच करें — अपनी उंगली को 2 इंच मिट्टी में डालें\n"
                "• अधिकांश सब्जियों को प्रति सप्ताह 1-1.5 इंच पानी की आवश्यकता होती है"
            )
        elif language == "gu":
            return (
                "જળ વ્યવસ્થાપન ટિપ્સ:\n"
                "• બાષ્પીભવન ઘટાડવા માટે વહેલી સવારે અથવા મોડી સાંજે પાણી આપો\n"
                "• ૩૦-૫૦% પાણીની બચત માટે ટપક સિંચાઈ પદ્ધતિનો ઉપયોગ કરો\n"
                "• ભેજ જાળવી રાખવા માટે છોડની આસપાસ મલ્ચિંગ કરો\n"
                "• પાણી આપતા પહેલા જમીનની ભેજ તપાસો — તમારી આંગળી ૨ ઇંચ સુધી જમીનમાં નાખી જુઓ\n"
                "• મોટાભાગના શાકભાજીને દર અઠવાડિયે ૧-૧.૫ ઇંચ પાણીની જરૂર હોય છે"
            )
        else:
            return (
                "Water management tips:\n"
                "• Water early morning or late evening to reduce evaporation\n"
                "• Use drip irrigation for 30-50% water savings\n"
                "• Mulch around plants to retain moisture\n"
                "• Check soil moisture before watering — stick your finger 2 inches into soil\n"
                "• Most vegetables need 1-1.5 inches of water per week"
            )

    def _handle_pest_query(self, message: str, kb_context: list, language: str) -> str:
        if language == "hi":
            return (
                "एकीकृत कीट प्रबंधन (आईपीएम) दृष्टिकोण:\n"
                "1. **रोकथाम:** फसल चक्र, प्रतिरोधी किस्में, स्वच्छ खेती\n"
                "2. **निगरानी:** नियमित निरीक्षण, चिपचिपे जाल, फेरोमोन जाल\n"
                "3. **जैविक नियंत्रण:** लाभकारी कीट (लेडीबग, लेसविंग), बीटी स्प्रे\n"
                "4. **जैविक उपचार:** नीम का तेल, कीटनाशक साबुन, डायटोमेसियस अर्थ\n"
                "5. **रासायनिक उपचार:** अंतिम उपाय के रूप में उपयोग करें, लेबल निर्देशों का पालन करें\n\n"
                "विशिष्ट पहचान के लिए अपने कैमरे से कीट को स्कैन करें!"
            )
        elif language == "gu":
            return (
                "સંકલિત જીવાત નિયંત્રણ (IPM) અભિગમ:\n"
                "૧. **રોકથામ:** પાકની ફેરબદલી, પ્રતિકારક જાતો, સ્વચ્છ ખેતી\n"
                "૨. **દેખરેખ:** નિયમિત નિરીક્ષણ, ચીકણા પિંજર, ફેરોમોન ટ્રેપ\n"
                "૩. **જૈવિક નિયંત્રણ:** ફાયદાકારક જીવજંતુઓ (લેડીબગ, લેસવિંગ), બીટી સ્પ્રે\n"
                "૪. **જૈવિક સારવાર:** લીમડાનું તેલ, જંતુનાશક સાબુ, ડાયટોમેસિયસ અર્થ\n"
                "૫. **રાસાયણિક સારવાર:** અંતિમ ઉપાય તરીકે જ વાપરો, લેબલ સૂચનાઓનું પાલન કરો\n\n"
                "ચોક્કસ જીવાત ઓળખવા માટે કેમેરા વડે સ્કેન કરો!"
            )
        else:
            return (
                "Integrated Pest Management (IPM) approach:\n"
                "1. **Prevention:** Crop rotation, resistant varieties, clean cultivation\n"
                "2. **Monitoring:** Regular scouting, sticky traps, pheromone traps\n"
                "3. **Biological:** Beneficial insects (ladybugs, lacewings), Bt spray\n"
                "4. **Organic:** Neem oil, insecticidal soap, diatomaceous earth\n"
                "5. **Chemical:** Use as last resort, follow label instructions\n\n"
                "Scan the pest with your camera for specific identification!"
            )

    def _handle_crop_query(self, message: str, kb_context: list, language: str) -> str:
        if language == "hi":
            return (
                "व्यक्तिगत फसल सिफारिशों के लिए, मैं निम्नलिखित पर विचार करता हूँ:\n"
                "• वर्तमान मौसम (खरीफ/रबी/जायद)\n"
                "• आपकी मिट्टी का प्रकार\n"
                "• स्थानीय तापमान और वर्षा\n"
                "• पानी की उपलब्धता\n\n"
                "विस्तृत सिफारिशों के लिए फसल सलाहकार (Crop Advisory) सुविधा का प्रयास करें!"
            )
        elif language == "gu":
            return (
                "વ્યક્તિગત પાક ભલામણો માટે, હું ધ્યાનમાં લઉં છું:\n"
                "• વર્તમાન ઋતુ (ખરીફ/રવિ/જાયદ)\n"
                "• તમારી જમીનનો પ્રકાર\n"
                "• સ્થાનિક તાપમાન અને વરસાદ\n"
                "• પાણીની ઉપલબ્ધતા\n\n"
                "વિગતવાર ભલામણો માટે પાક સલાહકાર સુવિધાનો ઉપયોગ કરો!"
            )
        else:
            return (
                "For personalized crop recommendations, I consider:\n"
                "• Current season (Kharif/Rabi/Zaid)\n"
                "• Your soil type\n"
                "• Local temperature and rainfall\n"
                "• Water availability\n\n"
                "Try the crop advisory feature for detailed recommendations!"
            )

    def _generate_suggestions(self, message: str, response: str, language: str = "en") -> list:
        """Generate follow-up question suggestions."""
        suggestions = []
        msg_lower = message.lower()

        if language == "hi":
            if "disease" in msg_lower or "रोग" in msg_lower:
                suggestions = ["जैविक उपचार बताओ", "रोकथाम कैसे करें?", "अधिक जानकारी"]
            elif "fertilizer" in msg_lower or "खाद" in msg_lower:
                suggestions = ["जैविक खाद के बारे में बताओ", "कब खाद डालें?", "मिट्टी परीक्षण"]
            else:
                suggestions = ["पौधा स्कैन करें", "मौसम देखें", "फसल सिफारिश"]
        elif language == "gu":
            if "disease" in msg_lower or "બીમારી" in msg_lower:
                suggestions = ["જૈવિક ઉપચાર જણાવો", "રોકથામ કેવી રીતે કરવી?", "વધુ વિગત"]
            else:
                suggestions = ["છોડ સ્કેન કરો", "હવામાન જુઓ", "પાક સલાહ"]
        else:
            if "disease" in msg_lower:
                suggestions = ["What's the organic treatment?", "How to prevent this disease?", "More Details"]
            elif "fertilizer" in msg_lower:
                suggestions = ["What about organic fertilizers?", "When should I fertilize?", "More Details"]
            elif "pest" in msg_lower:
                suggestions = ["Organic pest control?", "How to prevent pests?", "More Details"]
            elif "water" in msg_lower:
                suggestions = ["Drip irrigation setup?", "Rain water harvesting?", "More Details"]
            else:
                suggestions = ["Scan a plant", "Check weather", "Crop recommendations"]

        return suggestions[:3]

    def clear_history(self, session_id: str = None):
        """Clear conversation history."""
        if session_id:
            self._conversation_history.pop(session_id, None)
            self._session_context.pop(session_id, None)
        else:
            self._conversation_history.clear()
            self._session_context.clear()

    @property
    def is_ready(self) -> bool:
        return True
