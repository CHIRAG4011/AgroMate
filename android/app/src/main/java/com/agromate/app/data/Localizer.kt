package com.agromate.app.data

import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue

object Localizer {
    var currentLanguage by mutableStateOf("en")

    val supportedLanguages = listOf(
        LangItem("en", "English", "English"),
        LangItem("hi", "Hindi", "हिन्दी"),
        LangItem("gu", "Gujarati", "ગુજરાતી"),
        LangItem("mr", "Marathi", "मराठी"),
        LangItem("pa", "Punjabi", "ਪੰਜਾਬੀ"),
        LangItem("bn", "Bengali", "বাংলা"),
        LangItem("ta", "Tamil", "தமிழ்"),
        LangItem("te", "Telugu", "తెలుగు"),
        LangItem("kn", "Kannada", "ಕನ್ನಡ"),
        LangItem("ml", "Malayalam", "മലയാളം"),
        LangItem("or", "Odia", "ଓଡ଼ିଆ"),
        LangItem("as", "Assamese", "অসমীয়া"),
        LangItem("ur", "Urdu", "اردو"),
        LangItem("sa", "Sanskrit", "संस्कृतम्"),
        LangItem("kok", "Konkani", "कोंकणी"),
        LangItem("ne", "Nepali", "नेपाली"),
        LangItem("ks", "Kashmiri", "کٲشُر"),
        LangItem("mni", "Manipuri", "মৈতৈলোন্"),
        LangItem("sd", "Sindhi", "سنڌي"),
        LangItem("doi", "Dogri", "डोगरी"),
        LangItem("brx", "Bodo", "बर'"),
        LangItem("sat", "Santali", "ᱥᱟᱱᱛᱟᱲᱤ"),
        LangItem("mai", "Maithili", "मैथिली")
    )

    private val translations = mapOf(
        "welcome" to mapOf(
            "en" to "Welcome to AgroMate! I'm your AI farming assistant.",
            "hi" to "AgroMate में आपका स्वागत है! मैं आपका AI कृषि सहायक हूँ।",
            "gu" to "AgroMate માં આપનું સ્વાગત છે! હું તમારો AI ખેતી સહાયક છું."
        ),
        "nav_dashboard" to mapOf(
            "en" to "Dashboard", "hi" to "डैशबोर्ड", "gu" to "ડેશબોર્ડ",
            "mr" to "डॅशボード", "pa" to "ਡੈਸ਼ਬੋਰਡ", "bn" to "ড্যাশবোর্ড",
            "ta" to "டாஷ்போர்டு", "te" to "డ్యాష్‌బోర్డ్", "kn" to "ಡ್ಯಾಶ್‌ಬೋರ್ಡ್",
            "ml" to "ഡാഷ്‌ബോർഡ്", "or" to "ଡ୍ୟାସବୋର୍ଡ", "as" to "ডেছব'ৰ্ড",
            "ur" to "ڈیش بورڈ", "sa" to "नियन्त्रणपटलम्", "kok" to "डॅशबोर्ड",
            "ne" to "ड्यासबोर्ड", "ks" to "ڈیش بورڈ", "mni" to "ডেশবোর্ড",
            "sd" to "ڊيش بورڊ", "doi" to "डैशबोर्ड", "brx" to "डेसबर्ड",
            "sat" to "ᱰᱮᱥᱵᱚᱨᱰ", "mai" to "डैशबोर्ड"
        ),
        "nav_chat" to mapOf(
            "en" to "AI Chat", "hi" to "AI चैट", "gu" to "AI ચેટ",
            "mr" to "AI चॅट", "pa" to "AI ਚੈਟ", "bn" to "AI চযাট",
            "ta" to "AI அரடடை", "te" to "AI చాట", "kn" to "AI ਚাট",
            "ml" to "AI ചാററ", "or" to "AI ଚାଟ", "as" to "AI চেट",
            "ur" to "AI چیٹ", "sa" to "AI संवादः", "kok" to "AI गजाली",
            "ne" to "AI चयाट", "ks" to "AI چیٹ", "mni" to "AI চেৎ",
            "sd" to "AI چيٽ", "doi" to "AI चैट", "brx" to "AI गोसो",
            "sat" to "AI ᱨᱚᱲ", "mai" to "AI चैट"
        ),
        "nav_chatbot" to mapOf(
            "en" to "AI Chat", "hi" to "AI चैट", "gu" to "AI ચેટ",
            "mr" to "AI चॅट", "pa" to "AI ਚੈਟ", "bn" to "AI চযাট",
            "ta" to "AI அரடடை", "te" to "AI చాట", "kn" to "AI ਚাট",
            "ml" to "AI ചാററ", "or" to "AI ଚାଟ", "as" to "AI চেট",
            "ur" to "AI چیٹ", "sa" to "AI संवादः", "kok" to "AI गजाली",
            "ne" to "AI चयाट", "ks" to "AI چیٹ", "mni" to "AI চেৎ",
            "sd" to "AI چيٽ", "doi" to "AI चैट", "brx" to "AI गोसो",
            "sat" to "AI ᱨᱚᱲ", "mai" to "AI चैट"
        ),
        "nav_scan" to mapOf(
            "en" to "Scan", "hi" to "स्कैन", "gu" to "સ્કેન",
            "mr" to "स्कॅन", "pa" to "ਸਕੈਨ", "bn" to "স্ক্যান",
            "ta" to "ஸ்கேன்", "te" to "స్కాన్", "kn" to "ಸ್ಕ್ಯಾನ್",
            "ml" to "സ്കാൻ", "or" to "ସ୍କ୍ୟାନ", "as" to "স্কেন",
            "ur" to "اسکین", "sa" to "परीक्षा", "kok" to "स्कॅन",
            "ne" to "स्क्यान", "ks" to "اسکین", "mni" to "স্কেন",
            "sd" to "اسڪين", "doi" to "स्कैन", "brx" to "स्क्यान",
            "sat" to "ᱥᱠᱮᱱ", "mai" to "स्कैन"
        ),
        "nav_scan_diagnose" to mapOf(
            "en" to "Scan & Diagnose", "hi" to "स्कैन और निदान", "gu" to "સ્કેન અને નિદાન",
            "mr" to "स्कॅन आणि निदान", "pa" to "ਸਕੈਨ ਅਤੇ ਨਿਦਾਨ", "bn" to "স্ক্যান ও নির্ণয়",
            "ta" to "ஸ்கேன் & கண்டறிதல்", "te" to "స్కాన్ & నిర్ధారణ", "kn" to "ಸ್ಕ್ಯಾನ್ & ರೋಗನಿರ್ಣய",
            "ml" to "സ്കാൻ & രോഗനിർണയം", "or" to "ସ୍କ୍ୟାନ ଓ ନିଦାନ", "as" to "স্কেন আৰু নিদান",
            "ur" to "اسکین اور تشخیص", "sa" to "परीक्षा निदानम् च", "kok" to "स्कॅन आनी निदान",
            "ne" to "स्क्यान र निदान", "ks" to "اسکین تٕ تشخیص", "mni" to "স্কেন অমসুং নিদান",
            "sd" to "اسڪين ۽ تشخيص", "doi" to "स्कैन ते निदान", "brx" to "स्क्यान बिसान",
            "sat" to "ᱥᱠᱮᱱ ᱟᱨ ᱱᱤᱫᱟᱱ", "mai" to "स्कैन आ निदान"
        ),
        "nav_weather" to mapOf(
            "en" to "Weather", "hi" to "मौसम", "gu" to "હવામાન",
            "mr" to "हवामान", "pa" to "ਮੌਸਮ", "bn" to "আবহাওয়া",
            "ta" to "வானிலை", "te" to "వాతావరణం", "kn" to "ಹವಾಮಾನ",
            "ml" to "കാലാവസ്ഥ", "or" to "ପାଣିପाଗ", "as" to "বতৰ",
            "ur" to "موسم", "sa" to "जलवायुः", "kok" to "हवामान",
            "ne" to "मौसम", "ks" to "موسم", "mni" to "নুংশিৎ ফীভম",
            "sd" to "موسم", "doi" to "मौसम", "brx" to "बारहावा",
            "sat" to "ᱦᱚᱭᱼᱦᱤᱥᱤᱫ", "mai" to "मौसम"
        ),
        "type_message" to mapOf(
            "en" to "Type a message...", "hi" to "एक संदेश लिखें...", "gu" to "સંદેશ લખો...",
            "mr" to "संदेश लिहा...", "pa" to "ਇੱਕ ਸੁਨੇਹਾ ਟਾਈਪ ਕਰੋ...", "bn" to "একটি বার্তা লিখুন...",
            "ta" to "ஒரு செய்தியைத் தட்டச்சு செய்க...", "te" to "ఒక సందేశాన్ని టైప్ చేయండి...", "kn" to "ಒಂದು ಸಂದೇಶವನ್ನು ಟೈಪ್ ಮಾಡಿ...",
            "ml" to "ഒരു സന്ദേശം ടൈപ്പ് ചെയ്യുക...", "or" to "ଏକ ବାର୍ତ୍ତା ଟାଇପ୍ କରନ୍ତୁ...", "as" to "এটা বাৰ্তা লিখক...",
            "ur" to "ایک پیغام ٹائپ کریں...", "sa" to "संदेशं लिखत...", "kok" to "संदेश बरोवचो...",
            "ne" to "एक सन्देश टाइप गर्नुहोस्...", "ks" to "اکھ پیغام ٹائپ کٔरियो...", "mni" to "পাউজেল অমা টাইপ তৌ...",
            "sd" to "هڪ پيغام ٽائپ ڪريو...", "doi" to "इक सुनेहा टाइप करो...", "brx" to "संदेस टाइप खालाम...",
            "sat" to "ᱢᱮᱥᱮᱡᱽ ᱚᱞ ᱢᱮ...", "mai" to "एक संदेश टाइप करू..."
        ),
        "nav_settings" to mapOf(
            "en" to "Settings", "hi" to "सेटिंग्स", "gu" to "સેટિંગ્સ",
            "mr" to "सेटिंग्ज", "pa" to "ਸੈਟਿੰਗਾਂ", "bn" to "সেটিংস",
            "ta" to "அமைப்புகள்", "te" to "సెట్టింగ్‌లు", "kn" to "ಸೆಟ್ಟಿಂಗ್‌ಗಳು",
            "ml" to "ക്രമീകരണങ്ങൾ", "or" to "ସେଟିଂସ", "as" to "ছেটিংছ",
            "ur" to "ترتیبات", "sa" to "विन्यासाः", "kok" to "सेटिंग्ज",
            "ne" to "सेटिङहरू", "ks" to "سیٹِنگज", "mni" to "সেটিংস",
            "sd" to "سيٽنگون", "doi" to "सैटिंग्स", "brx" to "सेटिंस",
            "sat" to "ᱥᱮᱴᱤᱝ", "mai" to "सेटिंग्स"
        ),
        "nav_robots" to mapOf(
            "en" to "Robot Manager", "hi" to "रोबोट मैनेजर", "gu" to "રોબોટ મેનેજર",
            "mr" to "रोबोट व्यवस्थापक", "pa" to "ਰੋਬੋਟ ਮੈਨੇਜਰ", "bn" to "রোবট ম্যানেজার",
            "ta" to "ரோபோ மேலாளர்", "te" to "రోబోట్ మేనేజర్", "kn" to "ರೋಬೋಟ್ ಮ್ಯಾನೇಜರ್",
            "ml" to "റോബോട്ട് മാനേജർ", "or" to "ରୋବੋଟ ମ୍ୟାନେജର", "as" to "ৰবট মেনেজাৰ",
            "ur" to "روبوٹ مینیجر", "sa" to "यन्त्रप्रबन्धकः", "kok" to "रोबोट व्यवस्थापक",
            "ne" to "रोबोट प्रबन्धक", "ks" to "روبوٹ مینیجر", "mni" to "রোবোৎ মেনেজর",
            "sd" to "روبوٽ مينيجر", "doi" to "रोबोट मैनेजर", "brx" to "रोबोट बिसान",
            "sat" to "ᱨᱚᱵᱚᱴ ᱥᱮᱴᱤᱝ", "mai" to "रोबोट मैनेजर"
        ),
        "nav_history" to mapOf(
            "en" to "Scan History", "hi" to "स्कैन इतिहास", "gu" to "સ્કેન ઇતિહાસ",
            "mr" to "स्कॅन इतिहास", "pa" to "ਸਕੈਨ ਇਤਿਹਾਸ", "bn" to "স্ক্যান ইতিহাস",
            "ta" to "ஸ்கேன் வரலாறு", "te" to "స్కాన్ చరిత్ర", "kn" to "ಸ್ಕ್ಯಾನ್ ಇತಿಹಾಸ",
            "ml" to "സ്കാൻ ചരിത്രം", "or" to "ସ୍କ୍ୟାନ ଇତିହାସ", "as" to "স্কেন ইতিহাস",
            "ur" to "اسکین تاریخ", "sa" to "निदानवृत्तान्तः", "kok" to "स्कॅन इतिहास",
            "ne" to "स्क्यान इतिहास", "ks" to "اسکین تٲریخ", "mni" to "স্কেন পুৱারী",
            "sd" to "اسڪين تاريخ", "doi" to "स्कैन इतिहास", "brx" to "स्क्यान सोलों",
            "sat" to "ᱥᱠᱮᱱ ᱱᱟᱜᱟᱢ", "mai" to "स्कैन इतिहास"
        ),
        "nav_analytics" to mapOf(
            "en" to "Analytics", "hi" to "विश्लेषण", "gu" to "એનાલિટિક્સ",
            "mr" to "विश्लेषण", "pa" to "विश्लेषण", "bn" to "বিশ্লেষণ",
            "ta" to "பகுப்பாய்வு", "te" to "విశ్లేషణ", "kn" to "ವಿಶ್ಲೇಷಣೆ",
            "ml" to "വിശകലനം", "or" to "ବିଶ୍ଳେଷଣ", "as" to "বিশ্লেষণ",
            "ur" to "تجزیہ", "sa" to "विश्लेषणम्", "kok" to "विश्लेषण",
            "ne" to "विश्लेषण", "ks" to "تجزیہ", "mni" to "এনালাইতিক্স",
            "sd" to "تجزيي", "doi" to "विश्लेषण", "brx" to "विश्लेषण",
            "sat" to "ᱮᱱᱟᱞᱤᱴᱤᱠᱥ", "mai" to "विश्लेषण"
        ),
        "dashboard_total_scans" to mapOf(
            "en" to "Total Scans", "hi" to "कुल स्कैन", "gu" to "કુલ સ્કેન", "sd" to "ڪل اسڪين", "ur" to "کل اسکین"
        ),
        "dashboard_connected_robots" to mapOf(
            "en" to "Connected Robots", "hi" to "कनेक्टेड रोबोट", "gu" to "કનેક્ટેડ રોબોટ્સ", "sd" to "ڳنڍيل روبوٽ", "ur" to "منسلک روبوٹ"
        ),
        "dashboard_diseases_detected" to mapOf(
            "en" to "Diseases Detected", "hi" to "रोगों का पता चला", "gu" to "રોગો મળ્યા", "sd" to "بيمارين جو پتو پئجي ويو", "ur" to "بیماریوں کا پتہ چلا"
        ),
        "dashboard_conversations" to mapOf(
            "en" to "AI Conversations", "hi" to "AI बातचीत", "gu" to "AI વાતચીત", "sd" to "AI گفتگو", "ur" to "AI گفتگو"
        ),
        "temperature" to mapOf(
            "en" to "Temperature", "hi" to "तापमान", "gu" to "તાપમાન", "sd" to "دما", "ur" to "درجہ حرارت"
        ),
        "soil_moisture" to mapOf(
            "en" to "Soil Moisture", "hi" to "मिट्टी की नमी", "gu" to "જમીનની ભેજ", "sd" to "مٽي جي نمي", "ur" to "مٹی کی نمی"
        ),
        "alert_title" to mapOf(
            "en" to "Alert", "hi" to "चेतावनी", "gu" to "ચેતવણી", "sd" to "خبردار", "ur" to "انتباہ"
        ),
        "active_robot" to mapOf(
            "en" to "Active Robot Summary", "hi" to "सक्रिय रोबोट सारांश", "gu" to "સક્રિય રોબોટ સારાંશ", "sd" to "فعال روبوٽ جو خلاصو", "ur" to "فعال روبوٹ کا خلاصہ"
        ),
        "battery_level" to mapOf(
            "en" to "Battery Level", "hi" to "बैटरी स्तर", "gu" to "બેટરી સ્તર", "sd" to "بيٽري جي سطح", "ur" to "بیٹری کی سطح"
        ),
        "live_farm_telemetry" to mapOf(
            "en" to "Live Farm Video Telemetry", "hi" to "लाइव फार्म वीडियो टेलीमेट्री", "gu" to "લાઇવ ફાર્મ વિડિઓ ટેલિમેટ્રી", "sd" to "لايو فارم ويڊيو ٽيلي ميٽري", "ur" to "لائیو فارم ویڈیو ٹیلی میٹری"
        ),
        "quick_actions" to mapOf(
            "en" to "Quick Actions", "hi" to "त्वरित क्रियाएं", "gu" to "ઝડપી ક્રિયાઓ", "sd" to "جلدي ڪارناما", "ur" to "فوری اقدامات"
        ),
        "more_details" to mapOf(
            "en" to "More Details", "hi" to "अधिक जानकारी", "gu" to "વધુ વિગત",
            "mr" to "अधिक तपशील", "pa" to "ਹੋਰ ਵੇਰਵੇ", "bn" to "আরও বিস্তারিত",
            "ta" to "மேலும் விவரங்கள்", "te" to "మరిన్ని వివరాలు", "kn" to "ಹೆಚ್ಚಿನ ವಿವರಗಳು",
            "ml" to "കൂടുതൽ വിശദാംശങ്ങൾ", "or" to "ଅଧିକ ବିବରଣୀ", "as" to "অଧିକ ବିৱৰଣ",
            "ur" to "مزید تفصیلات", "sa" to "अधिकविवरणम्", "kok" to "अदिक तपशील",
            "ne" to "थप विवरण", "ks" to "مزید تفصیلٲت", "mni" to "অমুক ফংদোক্নবা",
            "sd" to "وڌيڪ تفصيلون", "doi" to "होर जानकारी", "brx" to "गोजोन बिसान",
            "sat" to "ᱡᱟᱹᱥᱛᱤ ᱵᱤᱵᱨᱚᱱ", "mai" to "अधिक जानकारी"
        ),
        "scan_now" to mapOf(
            "en" to "Scan Now", "hi" to "अभी स्कैन करें", "gu" to "હમણાં સ્કેન કરો",
            "mr" to "आता स्कॅन करा", "pa" to "ਹੁਣੇ ਸਕੈਨ ਕਰੋ", "bn" to "এখনই স্ক্যান করুন",
            "ta" to "இப்போது ஸ்கேன் செய்", "te" to "ఇప్పుడు స్కాన్ చేయండి",
            "kn" to "ಈಗ ಸ್ಕ್ಯಾನ್ ಮಾಡಿ", "ml" to "ഇപ്പോൾ സ്കാൻ ചെയ്യുക",
            "or" to "ବର୍ତ୍ତମାନ ସ୍କ୍ୟାନ କରନ୍ତୁ", "as" to "এতিয়া স্কেন কৰক",
            "ur" to "ابھی اسکین کریں", "sa" to "अधुना परीक्षां कुरुत", "kok" to "आतां स्कॅन करात",
            "ne" to "अहिले स्क्यान गर्नुहोस्", "ks" to "ابہِ اسکین کرِو",
            "mni" to "হৌজিক স্কেন তৌ", "sd" to "هاڻي اسڪين ڪريو", "doi" to "हुण स्कैन करो",
            "brx" to "दानो स्क्यान", "sat" to "ᱱᱤᱛ ᱥᱠᱮᱱ ᱢᱮ", "mai" to "अखन स्कैन करू"
        ),
        "server_ip" to mapOf(
            "en" to "Server IP Address", "hi" to "सर्वर आईपी एड्रेस", "gu" to "સર્વર આઈપી એડ્રેસ"
        ),
        "server_port" to mapOf(
            "en" to "Server Port", "hi" to "सर्वर पोर्ट", "gu" to "સર્વર પોર્ટ"
        ),
        "save_settings" to mapOf(
            "en" to "Save Settings", "hi" to "सेटिंग्स सहेजें", "gu" to "સેટિંગ્સ સાચવો"
        ),
        "app_preferences" to mapOf(
            "en" to "Application Preferences", "hi" to "एप्लिकेशन प्राथमिकताएं", "gu" to "એપ્લિકેશન પસંદગીઓ"
        ),
        "enable_push" to mapOf(
            "en" to "Enable Push Notifications", "hi" to "पुश सूचनाएं सक्षम करें", "gu" to "પુશ સૂચનાઓ સક્ષમ કરો"
        ),
        "auto_sync" to mapOf(
            "en" to "Background Auto Sync", "hi" to "पृष्ठभूमि स्वतः सिंक", "gu" to "બેકગ્રાઉન્ડ ઓટો સિંક"
        )
    )

    var dynamicTranslations by mutableStateOf<Map<String, String>>(emptyMap())

    suspend fun fetchTranslations(language: String) {
        try {
            val apiService = NetworkConfig.getApiService()
            val response = apiService.getTranslations(language)
            if (response.status == "success") {
                dynamicTranslations = response.translations
            }
        } catch (e: Exception) {
            e.printStackTrace()
        }
    }

    fun t(key: String): String {
        val dynamic = dynamicTranslations[key]
        if (dynamic != null) return dynamic

        val entry = translations[key] ?: return key
        return entry[currentLanguage] ?: entry["en"] ?: key
    }
}

data class LangItem(val code: String, val name: String, val native: String)
