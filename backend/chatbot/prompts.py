"""
AgroMate Chatbot Prompts — V2

System prompts and context builders for the AI chatbot.
Includes two-tier response system: short summary + detailed explanation on demand.
"""

from .knowledge_base import PlantKnowledgeBase
from ..voice.languages import SUPPORTED_LANGUAGES


SYSTEM_PROMPT = """You are AgroMate, a highly professional, authoritative, and intelligent AI Agricultural Expert developed by Sir Chirag.

Your core mission is to provide Indian farmers with precise, scientific, professional, and practical agronomic advice.

Core Domain Expertise:
1. Crop Science & Plant Pathology — Accurate crop identification, scientific naming, disease etiology, and treatment protocol.
2. Integrated Pest & Disease Management — Prioritizing organic/natural bio-control remedies first, followed by safe chemical recommendations.
3. Soil Health & Plant Nutrition — Fertilizer dosage, NPK ratios, micro-nutrient deficiencies, and soil conditioning.
4. Irrigation & Climate Advisory — Precision watering schedules, Kharif/Rabi/Zaid crop rotation, and weather risk management.

Communication Standards:
- Maintain an authoritative, professional, respectful, and highly competent expert tone.
- Deliver clear, well-structured responses using clean markdown formatting (bold section headers, clear bullet points, numbered step-by-step action plans).
- Always include actionable steps with specific dosages, application timings, and preventive measures.
- Respond in the exact language used by the user (Hindi, Gujarati, Marathi, English, etc.) with impeccable grammar and localized terminology.
- If asked about non-agricultural topics, answer politely and professionally while seamlessly connecting back to agronomic applications where relevant.
"""


SHORT_RESPONSE_PROMPT = """IMPORTANT RESPONSE FORMAT INSTRUCTIONS:
Provide ONLY a short summary response (2-4 simple sentences). Include:
- Plant/crop common name
- Scientific name (if known)
- Health status (healthy/diseased)
- Disease name (if detected)
- Most important immediate recommendation
- Organic remedy (if applicable)

Keep it extremely simple and farmer-friendly. This response will be read aloud via Text-to-Speech, so:
- Use short sentences
- Avoid complex technical terms
- Be direct and actionable
- Do NOT include detailed explanations, causes, prevention, or comprehensive treatment plans

Example good short response:
"This is a Tomato plant (Solanum lycopersicum). I detected Early Blight disease with moderate severity. Apply neem oil spray immediately and remove affected leaves. Water at the base of the plant, not on leaves."
"""


DETAILED_RESPONSE_PROMPT = """IMPORTANT RESPONSE FORMAT INSTRUCTIONS:
The user has requested a detailed explanation. Provide a COMPREHENSIVE analysis including ALL of the following sections:

## Plant Information
- Common name, Scientific name, Family
- Growth characteristics

## Disease Analysis (if detected)
- Disease name and type (fungal/bacterial/viral)
- Symptoms description
- Severity level
- Possible causes
- Spread risk assessment

## Treatment (in priority order)
### 1. Natural/Organic Remedies (HIGHEST PRIORITY)
- Specific organic solutions with quantities and application methods

### 2. Biological Control
- Beneficial organisms, companion planting

### 3. Chemical Treatment (ONLY if necessary)
- Specific chemicals with safety precautions
- Protective equipment needed
- Application schedule

## Prevention
- Cultural practices
- Resistant varieties
- Monitoring schedule

## Farming Recommendations
- Watering requirements (frequency and quantity)
- Fertilizer recommendations (organic first)
- Soil requirements
- Climate suitability
- Growth stage assessment
- Recovery timeline (if diseased)
- Expected yield impact
- Next inspection date

Format the response using clear markdown headers and bullet points.
"""


# ──────────────────────────────────────────────────────────────
# Language instruction map for all 23 languages
# ──────────────────────────────────────────────────────────────

LANGUAGE_INSTRUCTIONS = {
    "en": "Respond in English.",
    "hi": "कृपया हिंदी में जवाब दें। Respond in Hindi.",
    "gu": "કૃપા કરીને ગુજરાતીમાં જવાબ આપો. Respond in Gujarati.",
    "mr": "कृपया मराठीत उत्तर द्या. Respond in Marathi.",
    "pa": "ਕਿਰਪਾ ਕਰਕੇ ਪੰਜਾਬੀ ਵਿੱਚ ਜਵਾਬ ਦਿਓ। Respond in Punjabi.",
    "bn": "অনুগ্রহ করে বাংলায় উত্তর দিন। Respond in Bengali.",
    "ta": "தமிழில் பதிலளிக்கவும். Respond in Tamil.",
    "te": "దయచేసి తెలుగులో సమాధానం ఇవ్వండి. Respond in Telugu.",
    "kn": "ದಯವಿಟ್ಟು ಕನ್ನಡದಲ್ಲಿ ಉತ್ತರಿಸಿ. Respond in Kannada.",
    "ml": "ദയവായി മലയാളത്തിൽ ഉത്തരം നൽകുക. Respond in Malayalam.",
    "or": "ଦୟାକରି ଓଡ଼ିଆରେ ଉତ୍ତର ଦିଅନ୍ତୁ। Respond in Odia.",
    "as": "অনুগ্ৰহ কৰি অসমীয়াত উত্তৰ দিয়ক। Respond in Assamese.",
    "ur": "براہ کرم اردو میں جواب دیں۔ Respond in Urdu.",
    "sa": "कृपया संस्कृतेन उत्तरं ददातु। Respond in Sanskrit.",
    "kok": "उपकार करून कोंकणीत जाप दियात. Respond in Konkani.",
    "ne": "कृपया नेपालीमा जवाफ दिनुहोस्। Respond in Nepali.",
    "ks": "مہربٲنی کٔرِتھ کٲشُر مَنٛز جواب دِو۔ Respond in Kashmiri.",
    "mni": "চহি মৈতৈলোন্দা ৱাহনবীরকউ. Respond in Manipuri.",
    "sd": "مهرباني ڪري سنڌيءَ ۾ جواب ڏيو. Respond in Sindhi.",
    "doi": "मेहरबानी करियै डोगरी च जवाब द्यो। Respond in Dogri.",
    "brx": "अनुग्रह करून बर' आव जबाब हो. Respond in Bodo.",
    "sat": "ᱫᱟᱭᱟ ᱠᱟᱛᱮ ᱥᱟᱱᱛᱟᱲᱤ ᱨᱮ ᱛᱮᱞᱟ ᱢᱮ. Respond in Santali.",
    "mai": "कृपया मैथिली मे जवाब दियौक। Respond in Maithili.",
}


# ──────────────────────────────────────────────────────────────
# "More Details" intent detection keywords in all languages
# ──────────────────────────────────────────────────────────────

MORE_DETAILS_KEYWORDS = {
    "en": ["more details", "explain more", "tell me everything", "continue", "read the full report",
           "how do i treat it", "what should i do", "full analysis", "detailed explanation",
           "tell me more", "what about prevention", "how much water", "show complete analysis",
           "elaborate", "expand", "go deeper", "more info", "more information"],
    "hi": ["अधिक जानकारी", "और बताओ", "विस्तार से बताओ", "पूरी जानकारी", "इलाज बताओ",
           "क्या करना चाहिए", "रोकथाम बताओ", "पूरी रिपोर्ट", "सब कुछ बताओ", "आगे बताओ"],
    "gu": ["વધુ વિગત", "વધુ જણાવો", "વિસ્તારથી જણાવો", "સંપૂર્ણ માહિતી",
           "ઉપાય જણાવો", "શું કરવું", "બધું જણાવો", "આગળ જણાવો"],
    "mr": ["अधिक माहिती", "अजून सांगा", "विस्तृत माहिती", "उपचार सांगा",
           "काय करावे", "पूर्ण माहिती", "सविस्तर सांगा"],
    "ta": ["மேலும் விவரங்கள்", "மேலும் சொல்லுங்கள்", "விரிவாக சொல்லுங்கள்",
           "என்ன செய்ய வேண்டும்", "சிகிச்சை சொல்லுங்கள்"],
    "te": ["మరిన్ని వివరాలు", "మరింత చెప్పండి", "వివరంగా చెప్పండి",
           "ఏం చేయాలి", "చికిత్స చెప్పండి"],
    "bn": ["আরও বিস্তারিত", "আরও বলুন", "বিস্তারিত বলুন", "কি করব",
           "চিকিৎসা বলুন", "সব বলুন"],
    "pa": ["ਹੋਰ ਵੇਰਵੇ", "ਹੋਰ ਦੱਸੋ", "ਵਿਸਤਾਰ ਨਾਲ ਦੱਸੋ", "ਕੀ ਕਰਨਾ ਚਾਹੀਦਾ"],
    "kn": ["ಹೆಚ್ಚಿನ ವಿವರ", "ಇನ್ನಷ್ಟು ಹೇಳಿ", "ವಿವರವಾಗಿ ಹೇಳಿ"],
    "ml": ["കൂടുതൽ വിശദാംശങ്ങൾ", "കൂടുതൽ പറയൂ", "വിശദമായി പറയൂ"],
    "or": ["ଅଧିକ ବିବରଣୀ", "ଆହୁରି କୁହନ୍ତୁ", "ବିସ୍ତୃତ କୁହନ୍ତୁ"],
    "ur": ["مزید تفصیلات", "مزید بتائیں", "تفصیل سے بتائیں", "کیا کرنا چاہیے"],
    "ne": ["थप विवरण", "थप भन्नुहोस्", "विस्तारमा भन्नुहोस्"],
    "as": ["অধিক বিৱৰণ", "আৰু কওক", "বিস্তাৰিত কওক"],
}


def build_context_prompt(
    kb_results: list = None,
    scan_data: dict = None,
    language: str = "en",
    response_type: str = "summary",
) -> str:
    """
    Build a context prompt from knowledge base results and scan data.

    This provides the chatbot with relevant context for more
    accurate and specific responses.
    
    Args:
        kb_results: Knowledge base search results
        scan_data: Recent scan context data
        language: Response language code
        response_type: "summary" for short response, "detailed" for full report
    """
    context_parts = []

    # Language instruction
    lang_instruction = LANGUAGE_INSTRUCTIONS.get(language, LANGUAGE_INSTRUCTIONS["en"])
    context_parts.append(lang_instruction)

    # Response type instruction
    if response_type == "detailed":
        context_parts.append(DETAILED_RESPONSE_PROMPT)
    elif scan_data:
        context_parts.append(SHORT_RESPONSE_PROMPT)
    else:
        context_parts.append("""IMPORTANT INSTRUCTIONS FOR GENERAL CHAT:
Provide a clear, detailed, and comprehensive answer to the farmer's question.
- Format your response with clean markdown headings (###, ##), bullet points, and numbered lists where appropriate.
- Include practical step-by-step guidance, quantities, timing, and both organic & chemical options.
- Keep the tone warm, respectful, and helpful. Do NOT limit your answer to a short 2-sentence summary unless specifically asked to keep it brief.""")

    # Knowledge base context
    if kb_results:
        context_parts.append("\n## Relevant Knowledge:")
        for result in kb_results:
            context_parts.append(f"- **{result.get('topic', '')}**: {result.get('info', '')}")
            if result.get("treatment"):
                context_parts.append(f"  Treatment: {result['treatment']}")
            if result.get("organic"):
                context_parts.append(f"  Organic: {result['organic']}")

    # Recent scan context
    if scan_data:
        context_parts.append("\n## Recent Scan Results:")
        if scan_data.get("plant_name"):
            context_parts.append(f"- Plant: {scan_data['plant_name']}")
        if scan_data.get("scientific_name"):
            context_parts.append(f"- Scientific Name: {scan_data['scientific_name']}")
        if scan_data.get("disease_name"):
            context_parts.append(f"- Disease: {scan_data['disease_name']}")
        if scan_data.get("confidence"):
            conf = scan_data['confidence']
            if isinstance(conf, float) and conf <= 1.0:
                context_parts.append(f"- Confidence: {conf:.1%}")
            else:
                context_parts.append(f"- Confidence: {conf}")
        if scan_data.get("severity"):
            context_parts.append(f"- Severity: {scan_data['severity']}")
        if scan_data.get("is_healthy") is not None:
            status = "Healthy" if scan_data["is_healthy"] else "Diseased"
            context_parts.append(f"- Health Status: {status}")

    return "\n".join(context_parts)


def is_more_details_request(message: str, language: str = "en") -> bool:
    """Check if the user message is requesting more details about the previous response."""
    message_lower = message.lower().strip()
    
    # Check language-specific keywords
    for lang_code in [language, "en"]:  # Check both user's language and English
        keywords = MORE_DETAILS_KEYWORDS.get(lang_code, [])
        for keyword in keywords:
            if keyword in message_lower:
                return True
    
    # Generic patterns that work across languages
    generic_patterns = ["more", "detail", "explain", "elaborate", "continue",
                        "full", "complete", "treatment", "cure", "prevention"]
    match_count = sum(1 for p in generic_patterns if p in message_lower)
    
    return match_count >= 2  # At least 2 generic pattern matches
