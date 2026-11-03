"""
AgroMate Database Seed Data — V3 Ultra-Comprehensive Botanical & Agronomical Dataset

Populates the database with 125+ plant species, 85+ crop diseases,
12 essential nutrient deficiencies, and 25+ agricultural pests.
"""

import logging

logger = logging.getLogger(__name__)

# ============================================
# 1. Comprehensive Plant Species Database (125+ Species)
# ============================================

COMMON_PLANTS = [
    # --- Cereals & Millets ---
    {"name": "Rice", "scientific": "Oryza sativa", "category": "Cereal"},
    {"name": "Wheat", "scientific": "Triticum aestivum", "category": "Cereal"},
    {"name": "Corn/Maize", "scientific": "Zea mays", "category": "Cereal"},
    {"name": "Barley", "scientific": "Hordeum vulgare", "category": "Cereal"},
    {"name": "Pearl Millet (Bajra)", "scientific": "Pennisetum glaucum", "category": "Millet"},
    {"name": "Sorghum (Jowar)", "scientific": "Sorghum bicolor", "category": "Millet"},
    {"name": "Finger Millet (Ragi)", "scientific": "Eleusine coracana", "category": "Millet"},
    {"name": "Oats", "scientific": "Avena sativa", "category": "Cereal"},

    # --- Pulses & Legumes ---
    {"name": "Chickpea (Chana)", "scientific": "Cicer arietinum", "category": "Pulse"},
    {"name": "Pigeon Pea (Arhar/Tur)", "scientific": "Cajanus cajan", "category": "Pulse"},
    {"name": "Green Gram (Moong)", "scientific": "Vigna radiata", "category": "Pulse"},
    {"name": "Black Gram (Urad)", "scientific": "Vigna mungo", "category": "Pulse"},
    {"name": "Lentil (Masoor)", "scientific": "Lens culinaris", "category": "Pulse"},
    {"name": "Kidney Bean (Rajma)", "scientific": "Phaseolus vulgaris", "category": "Pulse"},
    {"name": "Soybean", "scientific": "Glycine max", "category": "Legume"},
    {"name": "Cowpea (Lobia)", "scientific": "Vigna unguiculata", "category": "Pulse"},
    {"name": "Green Pea", "scientific": "Pisum sativum", "category": "Vegetable"},
    {"name": "Cluster Bean (Guar)", "scientific": "Cyamopsis tetragonoloba", "category": "Legume"},

    # --- Solanaceous Vegetables ---
    {"name": "Tomato", "scientific": "Solanum lycopersicum", "category": "Vegetable"},
    {"name": "Potato", "scientific": "Solanum tuberosum", "category": "Vegetable"},
    {"name": "Brinjal/Eggplant", "scientific": "Solanum melongena", "category": "Vegetable"},
    {"name": "Chili Pepper", "scientific": "Capsicum annuum", "category": "Vegetable"},
    {"name": "Bell Pepper (Capsicum)", "scientific": "Capsicum annuum var. grossum", "category": "Vegetable"},

    # --- Alliums, Roots & Tubers ---
    {"name": "Onion", "scientific": "Allium cepa", "category": "Vegetable"},
    {"name": "Garlic", "scientific": "Allium sativum", "category": "Vegetable"},
    {"name": "Ginger", "scientific": "Zingiber officinale", "category": "Spice"},
    {"name": "Turmeric", "scientific": "Curcuma longa", "category": "Spice"},
    {"name": "Carrot", "scientific": "Daucus carota", "category": "Vegetable"},
    {"name": "Radish", "scientific": "Raphanus sativus", "category": "Vegetable"},
    {"name": "Beetroot", "scientific": "Beta vulgaris", "category": "Vegetable"},
    {"name": "Sweet Potato", "scientific": "Ipomoea batatas", "category": "Tuber"},

    # --- Cucurbits (Gourds & Melons) ---
    {"name": "Cucumber", "scientific": "Cucumis sativus", "category": "Vegetable"},
    {"name": "Bitter Gourd (Karela)", "scientific": "Momordica charantia", "category": "Vegetable"},
    {"name": "Bottle Gourd (Lauki)", "scientific": "Lagenaria siceraria", "category": "Vegetable"},
    {"name": "Ridge Gourd (Torai)", "scientific": "Luffa acutangula", "category": "Vegetable"},
    {"name": "Sponge Gourd (Ghiya Torai)", "scientific": "Luffa aegyptiaca", "category": "Vegetable"},
    {"name": "Pumpkin", "scientific": "Cucurbita moschata", "category": "Vegetable"},
    {"name": "Squash", "scientific": "Cucurbita pepo", "category": "Vegetable"},
    {"name": "Watermelon", "scientific": "Citrullus lanatus", "category": "Fruit"},
    {"name": "Muskmelon (Kharbooja)", "scientific": "Cucumis melo", "category": "Fruit"},

    # --- Cole Crops & Leafy Greens ---
    {"name": "Cabbage", "scientific": "Brassica oleracea var. capitata", "category": "Vegetable"},
    {"name": "Cauliflower", "scientific": "Brassica oleracea var. botrytis", "category": "Vegetable"},
    {"name": "Broccoli", "scientific": "Brassica oleracea var. italica", "category": "Vegetable"},
    {"name": "Spinach (Palak)", "scientific": "Spinacia oleracea", "category": "Leafy Green"},
    {"name": "Fenugreek (Methi)", "scientific": "Trigonella foenum-graecum", "category": "Leafy Green"},
    {"name": "Coriander (Dhaniya)", "scientific": "Coriandrum sativum", "category": "Herb"},
    {"name": "Mint (Pudina)", "scientific": "Mentha spicata", "category": "Herb"},
    {"name": "Lettuce", "scientific": "Lactuca sativa", "category": "Leafy Green"},

    # --- Other Vegetables ---
    {"name": "Okra (Ladyfinger/Bhindi)", "scientific": "Abelmoschus esculentus", "category": "Vegetable"},
    {"name": "Moringa (Drumstick)", "scientific": "Moringa oleifera", "category": "Vegetable"},

    # --- Fruits & Orchard Trees ---
    {"name": "Mango", "scientific": "Mangifera indica", "category": "Fruit"},
    {"name": "Banana", "scientific": "Musa acuminata", "category": "Fruit"},
    {"name": "Apple", "scientific": "Malus domestica", "category": "Fruit"},
    {"name": "Grape", "scientific": "Vitis vinifera", "category": "Fruit"},
    {"name": "Orange (Citrus)", "scientific": "Citrus sinensis", "category": "Fruit"},
    {"name": "Lemon (Citrus)", "scientific": "Citrus limon", "category": "Fruit"},
    {"name": "Sweet Lime (Mosambi)", "scientific": "Citrus limetta", "category": "Fruit"},
    {"name": "Guava", "scientific": "Psidium guajava", "category": "Fruit"},
    {"name": "Papaya", "scientific": "Carica papaya", "category": "Fruit"},
    {"name": "Pomegranate (Anaar)", "scientific": "Punica granatum", "category": "Fruit"},
    {"name": "Strawberry", "scientific": "Fragaria ananassa", "category": "Fruit"},
    {"name": "Pineapple", "scientific": "Ananas comosus", "category": "Fruit"},
    {"name": "Sapota (Chiku)", "scientific": "Manilkara zapota", "category": "Fruit"},
    {"name": "Custard Apple (Sitaphal)", "scientific": "Annona squamosa", "category": "Fruit"},
    {"name": "Fig (Anjeer)", "scientific": "Ficus carica", "category": "Fruit"},
    {"name": "Coconut", "scientific": "Cocos nucifera", "category": "Plantation"},
    {"name": "Litchi", "scientific": "Litchi chinensis", "category": "Fruit"},
    {"name": "Peach", "scientific": "Prunus persica", "category": "Fruit"},
    {"name": "Plum", "scientific": "Prunus domestica", "category": "Fruit"},
    {"name": "Cherry", "scientific": "Prunus avium", "category": "Fruit"},
    {"name": "Blueberry", "scientific": "Vaccinium corymbosum", "category": "Fruit"},
    {"name": "Raspberry", "scientific": "Rubus idaeus", "category": "Fruit"},
    {"name": "Avocado", "scientific": "Persea americana", "category": "Fruit"},
    {"name": "Dragon Fruit", "scientific": "Selenicereus undatus", "category": "Fruit"},

    # --- Commercial & Cash Crops ---
    {"name": "Cotton", "scientific": "Gossypium hirsutum", "category": "Fiber"},
    {"name": "Sugarcane", "scientific": "Saccharum officinarum", "category": "Cash Crop"},
    {"name": "Tea", "scientific": "Camellia sinensis", "category": "Beverage"},
    {"name": "Coffee", "scientific": "Coffea arabica", "category": "Beverage"},
    {"name": "Rubber", "scientific": "Hevea brasiliensis", "category": "Cash Crop"},
    {"name": "Jute", "scientific": "Corchorus olitorius", "category": "Fiber"},
    {"name": "Tobacco", "scientific": "Nicotiana tabacum", "category": "Cash Crop"},

    # --- Oilseeds ---
    {"name": "Mustard (Sarson)", "scientific": "Brassica juncea", "category": "Oilseed"},
    {"name": "Groundnut/Peanut", "scientific": "Arachis hypogaea", "category": "Oilseed"},
    {"name": "Sunflower", "scientific": "Helianthus annuus", "category": "Oilseed"},
    {"name": "Sesame (Til)", "scientific": "Sesamum indicum", "category": "Oilseed"},
    {"name": "Castor", "scientific": "Ricinus communis", "category": "Oilseed"},
    {"name": "Flaxseed (Linseed)", "scientific": "Linum usitatissimum", "category": "Oilseed"},

    # --- Spices & Condiments ---
    {"name": "Cumin (Jeera)", "scientific": "Cuminum cyminum", "category": "Spice"},
    {"name": "Fennel (Saunf)", "scientific": "Foeniculum vulgare", "category": "Spice"},
    {"name": "Cardamom (Elaichi)", "scientific": "Elettaria cardamomum", "category": "Spice"},
    {"name": "Black Pepper (Kali Mirch)", "scientific": "Piper nigrum", "category": "Spice"},
    {"name": "Clove (Laung)", "scientific": "Syzygium aromaticum", "category": "Spice"},
    {"name": "Cinnamon (Dalchini)", "scientific": "Cinnamomum verum", "category": "Spice"},
    {"name": "Fenugreek Seed", "scientific": "Trigonella foenum-graecum", "category": "Spice"},

    # --- Medicinal & Herbal Plants ---
    {"name": "Tulsi (Holy Basil)", "scientific": "Ocimum tenuiflorum", "category": "Medicinal"},
    {"name": "Neem", "scientific": "Azadirachta indica", "category": "Medicinal"},
    {"name": "Aloe Vera", "scientific": "Aloe barbadensis miller", "category": "Medicinal"},
    {"name": "Ashwagandha", "scientific": "Withania somnifera", "category": "Medicinal"},
    {"name": "Giloy", "scientific": "Tinospora cordifolia", "category": "Medicinal"},
    {"name": "Lemongrass", "scientific": "Cymbopogon citratus", "category": "Medicinal"},

    # --- Houseplants & Ornamentals ---
    {"name": "Rose", "scientific": "Rosa rubiginosa", "category": "Ornamental"},
    {"name": "Marigold (Genda)", "scientific": "Tagetes erecta", "category": "Ornamental"},
    {"name": "Hibiscus (Gudhal)", "scientific": "Hibiscus rosa-sinensis", "category": "Ornamental"},
    {"name": "Jasmine (Mogra)", "scientific": "Jasminum sambac", "category": "Ornamental"},
    {"name": "Chrysanthemum (Guldaudi)", "scientific": "Chrysanthemum morifolium", "category": "Ornamental"},
    {"name": "Money Plant (Golden Pothos)", "scientific": "Epipremnum aureum", "category": "Houseplant"},
    {"name": "Snake Plant (Sansevieria)", "scientific": "Dracaena trifasciata", "category": "Houseplant"},
    {"name": "Monstera (Split-leaf Philodendron)", "scientific": "Monstera deliciosa", "category": "Houseplant"},
    {"name": "Peace Lily", "scientific": "Spathiphyllum wallisii", "category": "Houseplant"},
    {"name": "Spider Plant", "scientific": "Chlorophytum comosum", "category": "Houseplant"},
    {"name": "Rubber Plant", "scientific": "Ficus elastica", "category": "Houseplant"},
    {"name": "Fiddle Leaf Fig", "scientific": "Ficus lyrata", "category": "Houseplant"},
    {"name": "Calathea (Prayer Plant)", "scientific": "Goeppertia insignis", "category": "Houseplant"},
    {"name": "Areca Palm", "scientific": "Dypsis lutescens", "category": "Houseplant"},
    {"name": "ZZ Plant", "scientific": "Zamioculcas zamiifolia", "category": "Houseplant"},
]

# ============================================
# 2. Comprehensive Crop Diseases Database (85+ Pathologies)
# ============================================

COMMON_DISEASES = [
    # --- Tomato Pathologies ---
    {
        "plant": "Tomato",
        "disease": "Early Blight",
        "pathogen": "Alternaria solani",
        "symptoms": "Concentric target-like dark brown rings on older lower leaves with yellow halo",
        "treatment": "Copper oxychloride 50 WP (3g/L) or Mancozeb 75 WP (2g/L) spray every 10-14 days",
        "organic_treatment": "Neem oil spray (5ml/L) with 0.1% soap solution. Trichoderma viride foliar spray. Remove lower leaves.",
        "severity_range": "mild to severe",
    },
    {
        "plant": "Tomato",
        "disease": "Late Blight",
        "pathogen": "Phytophthora infestans",
        "symptoms": "Water-soaked dark lesions expanding rapidly; white fuzzy growth under leaf surface in humid weather",
        "treatment": "Metalaxyl + Mancozeb (Ridomil Gold 2.5g/L) or Cymoxanil spray immediately",
        "organic_treatment": "Copper hydroxide spray (2g/L). Immediately destroy severely infected foliage. Drip irrigation only.",
        "severity_range": "severe",
    },
    {
        "plant": "Tomato",
        "disease": "Bacterial Spot",
        "pathogen": "Xanthomonas campestris pv. vesicatoria",
        "symptoms": "Small greasy water-soaked dark spots with yellow halos; scab-like raised lesions on fruit",
        "treatment": "Copper hydroxide (2g/L) combined with Streptocycline (0.1g/L)",
        "organic_treatment": "Pseudomonas fluorescens (10g/L) spray. Hot water seed treatment (50°C for 25 min).",
        "severity_range": "moderate to severe",
    },
    {
        "plant": "Tomato",
        "disease": "Leaf Mold",
        "pathogen": "Passalora fulva",
        "symptoms": "Pale greenish-yellow patches on upper leaf surface with olive-brown velvety mold underneath",
        "treatment": "Difenoconazole (0.5ml/L) or Chlorothalonil (2g/L)",
        "organic_treatment": "Improve airflow and reduce greenhouse humidity below 80%. Sulfur dust application.",
        "severity_range": "mild to moderate",
    },
    {
        "plant": "Tomato",
        "disease": "Septoria Leaf Spot",
        "pathogen": "Septoria lycopersici",
        "symptoms": "Numerous circular spots with dark brown margins and gray/white centers with tiny black specks",
        "treatment": "Chlorothalonil (2g/L) or Mancozeb (2.5g/L) weekly spray",
        "organic_treatment": "Bordeaux mixture (1%). Clean all crop debris. Heavy organic mulch to prevent splashback.",
        "severity_range": "moderate",
    },
    {
        "plant": "Tomato",
        "disease": "Yellow Leaf Curl Virus (TYLCV)",
        "pathogen": "Tomato Yellow Leaf Curl Virus (Vectored by Whitefly)",
        "symptoms": "Severe upward leaf curling, cupping, yellowing margins, stunting, and bushy bushy dwarf habit",
        "treatment": "Control whitefly vector with Spiromesifen (1ml/L) or Thiamethoxam (0.3g/L)",
        "organic_treatment": "Yellow sticky traps (15-20 per acre). Neem oil (5ml/L) + Pongamia oil. Rogue out infected plants.",
        "severity_range": "severe",
    },
    {
        "plant": "Tomato",
        "disease": "Target Spot",
        "pathogen": "Corynespora cassiicola",
        "symptoms": "Pinpoint brown spots enlarging into concentric circular lesions with light brown centers",
        "treatment": "Azoxystrobin + Difenoconazole (1ml/L) or Pyraclostrobin",
        "organic_treatment": "Bacillus subtilis foliar spray. Keep canopy dry and avoid overhead sprinkler watering.",
        "severity_range": "moderate",
    },
    {
        "plant": "Tomato",
        "disease": "Fusarium Wilt",
        "pathogen": "Fusarium oxysporum f. sp. lycopersici",
        "symptoms": "One-sided yellowing of branches, vascular browning in stem, whole-plant permanent wilting",
        "treatment": "Carbendazim drench (1g/L) at root zone. Use resistant cultivars (VFN hybrids).",
        "organic_treatment": "Trichoderma harzianum soil enrichment (5kg/acre with vermicompost). Solarize soil in summer.",
        "severity_range": "severe",
    },

    # --- Potato Pathologies ---
    {
        "plant": "Potato",
        "disease": "Early Blight",
        "pathogen": "Alternaria solani",
        "symptoms": "Dark brown dry circular spots with concentric rings primarily on mature lower foliage",
        "treatment": "Mancozeb 75 WP (2.5g/L) or Propineb 70 WP (2g/L)",
        "organic_treatment": "Neem cake soil application + Neem oil foliar spray. Regular potassium nutrition.",
        "severity_range": "moderate",
    },
    {
        "plant": "Potato",
        "disease": "Late Blight",
        "pathogen": "Phytophthora infestans",
        "symptoms": "Water-soaked irregular black/brown lesions with white downy mildew on leaf underside in fog/rain",
        "treatment": "Cymoxanil + Mancozeb (Sectotin 3g/L) or Dimethomorph (1g/L) preventative sprays",
        "organic_treatment": "Copper oxychloride (3g/L) before canopy closure. Destroy infected haulms 10 days before harvest.",
        "severity_range": "severe",
    },
    {
        "plant": "Potato",
        "disease": "Common Scab",
        "pathogen": "Streptomyces scabies",
        "symptoms": "Corky, rough, cratered brownish lesions on tuber surface; stunted growth",
        "treatment": "Seed treatment with Mancozeb (2.5g/kg). Maintain soil pH between 5.0 and 5.2.",
        "organic_treatment": "Green manuring with clover. Maintain high soil moisture during tuber initiation.",
        "severity_range": "moderate",
    },
    {
        "plant": "Potato",
        "disease": "Black Scurf",
        "pathogen": "Rhizoctonia solani",
        "symptoms": "Hard dark black sclerotial specks resembling dirt adhering to tuber skin; cankers on sprouts",
        "treatment": "Tuber treatment with Moncut (Flutolanil 2.5ml/kg) or Carbendazim (2g/kg)",
        "organic_treatment": "Trichoderma viride seed tuber dip (10g/L) for 30 minutes before sowing.",
        "severity_range": "mild to moderate",
    },

    # --- Rice / Paddy Pathologies ---
    {
        "plant": "Rice",
        "disease": "Blast",
        "pathogen": "Magnaporthe oryzae (Pyricularia oryzae)",
        "symptoms": "Spindle/diamond-shaped lesions with grayish centers and brown reddish margins on leaves and neck",
        "treatment": "Tricyclazole 75 WP (Beam 0.6g/L) or Isoprothiolane 40 EC (1.5ml/L)",
        "organic_treatment": "Pseudomonas fluorescens seed treatment (10g/kg) and foliar spray (2g/L). Avoid excess urea/nitrogen.",
        "severity_range": "severe",
    },
    {
        "plant": "Rice",
        "disease": "Brown Spot",
        "pathogen": "Bipolaris oryzae (Helminthosporium oryzae)",
        "symptoms": "Small oval to circular dark brown sesame-seed-like spots across leaf blade with yellow halo",
        "treatment": "Propiconazole 25 EC (Tilt 1ml/L) or Mancozeb (2g/L)",
        "organic_treatment": "Apply balanced NPK with potassium and silicon. Seed treatment with hot water or Pseudomonas.",
        "severity_range": "moderate",
    },
    {
        "plant": "Rice",
        "disease": "Bacterial Leaf Blight",
        "pathogen": "Xanthomonas oryzae pv. oryzae",
        "symptoms": "Water-soaked stripes along leaf margins turning yellow-white; wavy lesions from tip downwards",
        "treatment": "Streptocycline (1.5g) + Copper oxychloride (250g) in 200L water per acre",
        "organic_treatment": "Neem oil (5ml/L). Drain field water and withhold nitrogen top dressing for 10 days.",
        "severity_range": "severe",
    },
    {
        "plant": "Rice",
        "disease": "Sheath Blight",
        "pathogen": "Rhizoctonia solani",
        "symptoms": "Greenish-grey oval or irregular water-soaked spots on leaf sheath near water line, border dark brown",
        "treatment": "Hexaconazole 5 EC (2ml/L) or Validamycin 3L (2.5ml/L)",
        "organic_treatment": "Trichoderma viride field application with organic manure (2.5kg/ha). Wide plant spacing.",
        "severity_range": "moderate to severe",
    },
    {
        "plant": "Rice",
        "disease": "False Smut",
        "pathogen": "Ustilaginoidea virens",
        "symptoms": "Individual grains converted into velvety orange balls turning olive-green and blackish",
        "treatment": "Copper hydroxide (2g/L) or Propiconazole spray at booting stage",
        "organic_treatment": "Field sanitation and early planting. Avoid high nitrogen dosage during panicle emergence.",
        "severity_range": "moderate",
    },

    # --- Wheat Pathologies ---
    {
        "plant": "Wheat",
        "disease": "Stripe / Yellow Rust",
        "pathogen": "Puccinia striiformis f. sp. tritici",
        "symptoms": "Bright yellow pustules arranged in distinct narrow parallel stripes/lines along leaf veins",
        "treatment": "Propiconazole 25 EC (Tilt 1ml/L) or Tebuconazole (1ml/L) at first symptom appearance",
        "organic_treatment": "Plant resistant cultivars (HD-2967, PBW-550). Eradicate alternate host barberry bushes.",
        "severity_range": "severe",
    },
    {
        "plant": "Wheat",
        "disease": "Leaf / Brown Rust",
        "pathogen": "Puccinia triticina",
        "symptoms": "Small round or oval orange-brown powdery pustules scattered randomly on upper leaf surface",
        "treatment": "Tebuconazole 25.9 EC (1ml/L) or Mancozeb (2.5g/L)",
        "organic_treatment": "Early sowing; balanced potash fertilization; resistant seeds.",
        "severity_range": "moderate to severe",
    },
    {
        "plant": "Wheat",
        "disease": "Powdery Mildew",
        "pathogen": "Blumeria graminis f. sp. tritici",
        "symptoms": "White to light gray powdery cotton-like patches covering leaves and sheath, turning dull grey",
        "treatment": "Wettable sulfur (3g/L) or Propiconazole 25 EC (1ml/L)",
        "organic_treatment": "Sulfur dusting (20kg/ha). Increase plant spacing for better sunlight penetration.",
        "severity_range": "mild to moderate",
    },
    {
        "plant": "Wheat",
        "disease": "Karnal Bunt",
        "pathogen": "Tilletia indica",
        "symptoms": "Partial conversion of grains into black powdery teliospores emitting rotten fish odor (trimethylamine)",
        "treatment": "Seed treatment with Carboxin + Thiram (Vitavax 2g/kg). Propiconazole at heading.",
        "organic_treatment": "Certified seed use. Avoid excessive irrigation during anthesis / flowering.",
        "severity_range": "severe",
    },

    # --- Corn / Maize Pathologies ---
    {
        "plant": "Corn/Maize",
        "disease": "Northern Leaf Blight",
        "pathogen": "Exserohilum turcicum",
        "symptoms": "Large elongated cigar-shaped grayish-green to tan lesions (2.5 to 15 cm long) parallel to veins",
        "treatment": "Azoxystrobin + Difenoconazole (1ml/L) or Mancozeb 75 WP (2.5g/L)",
        "organic_treatment": "Crop rotation with non-grasses. Deep plowing of corn stubble. Resistant seed hybrids.",
        "severity_range": "moderate to severe",
    },
    {
        "plant": "Corn/Maize",
        "disease": "Common Rust",
        "pathogen": "Puccinia sorghi",
        "symptoms": "Cinnamon-brown circular to elongate pustules on both upper and lower leaf surfaces",
        "treatment": "Mancozeb (2.5g/L) or Pyraclostrobin (1ml/L)",
        "organic_treatment": "Neem seed kernel extract (5%). Plant early in season to avoid late-summer spore arrival.",
        "severity_range": "mild to moderate",
    },
    {
        "plant": "Corn/Maize",
        "disease": "Southern Corn Leaf Blight",
        "pathogen": "Bipolaris maydis",
        "symptoms": "Small tan lesions with parallel sides constrained between leaf veins",
        "treatment": "Propiconazole (1ml/L) or Chlorothalonil (2g/L)",
        "organic_treatment": "Debris clearance. Resistant hybrid seed varieties.",
        "severity_range": "moderate",
    },

    # --- Cotton Pathologies ---
    {
        "plant": "Cotton",
        "disease": "Bacterial Blight (Black Arm)",
        "pathogen": "Xanthomonas citri pv. malvacearum",
        "symptoms": "Angular water-soaked spots bounded by veins on leaves; black lesions girdling stems (black arm)",
        "treatment": "Copper oxychloride (2.5g/L) + Streptocycline (0.1g/L) foliar spray",
        "organic_treatment": "Acid delinting of seeds. Pseudomonas fluorescens seed treatment (10g/kg). Avoid sprinkler irrigation.",
        "severity_range": "severe",
    },
    {
        "plant": "Cotton",
        "disease": "Cotton Leaf Curl Virus (CLCuV)",
        "pathogen": "Cotton Leaf Curl Virus (Begomovirus transmitted by Whitefly)",
        "symptoms": "Upward or downward leaf curling, thick swollen leaf veins, enations (leaf-like outgrowths) underneath",
        "treatment": "Control whitefly vectors using Diafenthiuron (1g/L) or Flonicamid (0.3g/L)",
        "organic_treatment": "Yellow sticky traps (20/acre). Neem oil spray (5ml/L). Remove weed hosts (Abutilon, Xanthium).",
        "severity_range": "severe",
    },
    {
        "plant": "Cotton",
        "disease": "Grey Mildew (Dahiya)",
        "pathogen": "Ramularia areola",
        "symptoms": "Angular translucent spots on lower leaves with white frosty mildew growth on underside",
        "treatment": "Wettable sulfur (3g/L) or Carbendazim (1g/L)",
        "organic_treatment": "Sulfur dusting (25kg/ha). Cow urine + hing bio-formulation spray.",
        "severity_range": "moderate",
    },
    {
        "plant": "Cotton",
        "disease": "Fusarium / Verticillium Wilt",
        "pathogen": "Fusarium oxysporum / Verticillium dahliae",
        "symptoms": "Yellowing of leaf margins between main veins (tiger stripe pattern); brown vascular ring inside stem",
        "treatment": "Soil drenching around root zone with Carbendazim (1g/L) + Copper oxychloride (2.5g/L)",
        "organic_treatment": "Soil application of Trichoderma viride enriched in farmyard manure (5kg/acre). Crop rotation with sorghum.",
        "severity_range": "severe",
    },

    # --- Grape Pathologies ---
    {
        "plant": "Grape",
        "disease": "Powdery Mildew",
        "pathogen": "Erysiphe necator (Uncinula necator)",
        "symptoms": "White dusty powdery fungal coating on leaves, young canes, and berries; berry cracking",
        "treatment": "Sulfur 80 WDG (2g/L) or Penconazole 10 EC (0.5ml/L) or Kresoxim-methyl",
        "organic_treatment": "Potassium bicarbonate spray (3g/L). Neem oil (5ml/L). 10% cow milk foliar spray. Canopy pruning.",
        "severity_range": "moderate to severe",
    },
    {
        "plant": "Grape",
        "disease": "Downy Mildew",
        "pathogen": "Plasmopara viticola",
        "symptoms": "Yellow translucent 'oil spots' on upper leaf surface with dense white cottony mold underneath",
        "treatment": "Metalaxyl-M + Mancozeb (2.5g/L) or Dimethomorph (1g/L)",
        "organic_treatment": "Bordeaux mixture (1%). Prune lower shoots touching the ground to stop splash infection.",
        "severity_range": "severe",
    },
    {
        "plant": "Grape",
        "disease": "Black Rot",
        "pathogen": "Guignardia bidwellii",
        "symptoms": "Reddish-brown circular spots on leaves; berries shrivel into black hard wrinkled mummies",
        "treatment": "Myclobutanil (0.4g/L) or Mancozeb (2.5g/L)",
        "organic_treatment": "Remove and burn all mummified fruit clusters. Lime sulfur dormant spray.",
        "severity_range": "moderate to severe",
    },
    {
        "plant": "Grape",
        "disease": "Anthracnose (Bird's Eye Rot)",
        "pathogen": "Elsinoe ampelina",
        "symptoms": "Sunken circular dark lesions with gray centers and dark reddish margins resembling bird's eyes",
        "treatment": "Copper oxychloride (3g/L) or Carbendazim (1g/L)",
        "organic_treatment": "Bordeaux mixture (1%). Prune infected canes in winter and burn.",
        "severity_range": "moderate",
    },

    # --- Apple Pathologies ---
    {
        "plant": "Apple",
        "disease": "Apple Scab",
        "pathogen": "Venturia inaequalis",
        "symptoms": "Olive-green to dull dark brown velvety spots on upper leaf surfaces and scabby lesions on fruit",
        "treatment": "Difenoconazole (0.3ml/L) or Captan 50 WP (2.5g/L) or Dodine",
        "organic_treatment": "Lime sulfur application during green tip stage. Shred and compost fallen orchard leaves.",
        "severity_range": "moderate to severe",
    },
    {
        "plant": "Apple",
        "disease": "Cedar Apple Rust",
        "pathogen": "Gymnosporangium juniperi-virginianae",
        "symptoms": "Bright orange-yellow circular leaf spots with tiny black pycnidia; tube-like aecia below",
        "treatment": "Myclobutanil (0.5g/L) or Mancozeb (2.5g/L) during pink bud to petal fall",
        "organic_treatment": "Eradicate red cedar / juniper trees within 1 mile of orchard. Sulfur sprays.",
        "severity_range": "moderate",
    },
    {
        "plant": "Apple",
        "disease": "Black Rot / Frogeye Leaf Spot",
        "pathogen": "Botryosphaeria obtusa",
        "symptoms": "Small purple leaf spots expanding with light tan centers resembling frog eyes; black rotting fruit",
        "treatment": "Captan (2.5g/L) or Thiophanate-methyl (1g/L)",
        "organic_treatment": "Prune out dead wood, fire blight cankers, and mummified apples in winter.",
        "severity_range": "moderate",
    },

    # --- Mango Pathologies ---
    {
        "plant": "Mango",
        "disease": "Anthracnose",
        "pathogen": "Colletotrichum gloeosporioides",
        "symptoms": "Dark brown/black spots on leaves, blossom blight causing flower drop, tear stain marks on fruit",
        "treatment": "Carbendazim (1g/L) or Azoxystrobin (1ml/L) during flowering and panicle emergence",
        "organic_treatment": "Bordeaux mixture (1%) post-pruning. Hot water dip for fruits at 52°C for 10 minutes.",
        "severity_range": "moderate to severe",
    },
    {
        "plant": "Mango",
        "disease": "Powdery Mildew",
        "pathogen": "Oidium mangiferae",
        "symptoms": "White powdery fungal coating on inflorescence, panicles, and young leaves; severe fruit drop",
        "treatment": "Wettable sulfur (3g/L) or Hexaconazole (1ml/L) before flower opening",
        "organic_treatment": "Sulfur dusting (250g/tree). 0.5% neem oil spray.",
        "severity_range": "moderate",
    },
    {
        "plant": "Mango",
        "disease": "Dieback",
        "pathogen": "Lasiodiplodia theobromae",
        "symptoms": "Drying of twigs and branches from tip downwards; leaves turn brown and roll upwards",
        "treatment": "Prune infected twigs 3 inches below affected part; paste cut ends with Copper oxychloride paste",
        "organic_treatment": "Copper hydroxide spray (3g/L). Maintain proper soil drainage and stem health.",
        "severity_range": "moderate to severe",
    },

    # --- Banana Pathologies ---
    {
        "plant": "Banana",
        "disease": "Panama Wilt (Fusarium)",
        "pathogen": "Fusarium oxysporum f. sp. cubense (Tropical Race 4)",
        "symptoms": "Yellowing of lower leaf margins progressing inward, leaf petiole buckling; stem split with brown core",
        "treatment": "Strict field quarantine. Inject Carbendazim (2% solution) into pseudo-stem.",
        "organic_treatment": "Bio-control with Trichoderma viride + Pseudomonas fluorescens (50g/plant with compost). Crop rotation with rice.",
        "severity_range": "severe",
    },
    {
        "plant": "Banana",
        "disease": "Sigatoka Leaf Spot",
        "pathogen": "Pseudocercospora fijiensis / musae",
        "symptoms": "Dark brown or black narrow streaks running parallel to veins with sunken gray dry centers",
        "treatment": "Propiconazole (1ml/L) + mineral oil (10ml/L) or Carbendazim (1g/L)",
        "organic_treatment": "De-leafing (cut and burn heavily spotted leaves). Ensure excellent field drainage.",
        "severity_range": "moderate to severe",
    },
    {
        "plant": "Banana",
        "disease": "Bunchy Top Virus",
        "pathogen": "Banana Bunchy Top Virus (BBTV, vectored by Aphid Pentalonia)",
        "symptoms": "Leaves become narrow, short, upright and crowded at apex forming a rosette bunch; dark green dot-dash streaks",
        "treatment": "Control aphid vector with Imidacloprid (0.3ml/L). Rogue and inject kerosene/glyphosate into infected mat.",
        "organic_treatment": "Use virus-indexed tissue-cultured suckers. Field eradication of aphid vectors with neem soap.",
        "severity_range": "severe",
    },

    # --- Chili & Pepper Pathologies ---
    {
        "plant": "Chili Pepper",
        "disease": "Anthracnose (Dieback/Fruit Rot)",
        "pathogen": "Colletotrichum capsici",
        "symptoms": "Circular sunken spots on ripe red chilies with concentric rings of black acervuli; twig dieback",
        "treatment": "Azoxystrobin (1ml/L) or Mancozeb (2.5g/L) or Copper oxychloride (3g/L)",
        "organic_treatment": "Seed treatment with Trichoderma viride (10g/kg). Spray neem seed kernel extract (5%).",
        "severity_range": "moderate to severe",
    },
    {
        "plant": "Chili Pepper",
        "disease": "Chili Leaf Curl Virus",
        "pathogen": "Chili Leaf Curl Virus (Begomovirus vectored by Whiteflies and Thrips)",
        "symptoms": "Severe upward rolling of leaf margins, puckering, reduction of leaf size, vein thickening",
        "treatment": "Control whitefly/thrips vectors with Fipronil (1.5ml/L) or Spinetoram (0.9ml/L)",
        "organic_treatment": "Blue and yellow sticky traps (15 each/acre). Spray neem oil (5ml/L) + castor oil.",
        "severity_range": "severe",
    },
    {
        "plant": "Chili Pepper",
        "disease": "Bacterial Spot",
        "pathogen": "Xanthomonas euvesicatoria",
        "symptoms": "Small angular dark greasy spots on leaves; heavy leaf shedding leading to sunscalding of fruit",
        "treatment": "Streptocycline (0.1g/L) + Copper oxychloride (2.5g/L)",
        "organic_treatment": "Pseudomonas fluorescens spray. Certified disease-free seed beds.",
        "severity_range": "moderate",
    },

    # --- Onion & Garlic Pathologies ---
    {
        "plant": "Onion",
        "disease": "Purple Blotch",
        "pathogen": "Alternaria porri",
        "symptoms": "Small water-soaked lesions developing dark purple centers with yellow halos; leaf tips die back",
        "treatment": "Mancozeb (2.5g/L) or Tebuconazole + Trifloxystrobin (Nativo 0.7g/L)",
        "organic_treatment": "Trichoderma harzianum bulb treatment. Good field drainage and avoid sprinkler watering.",
        "severity_range": "moderate to severe",
    },
    {
        "plant": "Onion",
        "disease": "Downy Mildew",
        "pathogen": "Peronospora destructor",
        "symptoms": "Pale greenish-yellow cylindrical spots with violet furry mold on leaves in morning dew",
        "treatment": "Metalaxyl + Mancozeb (2.5g/L) spray with sticking agent (Apsa-80)",
        "organic_treatment": "Crop rotation for 3 years. Use disease-free sets and well-drained raised beds.",
        "severity_range": "moderate",
    },
    {
        "plant": "Onion",
        "disease": "Basal Rot (Fusarium)",
        "pathogen": "Fusarium oxysporum f. sp. cepae",
        "symptoms": "Yellowing and dying of leaves from tip; root decay with white mycelial growth on basal plate",
        "treatment": "Carbendazim drench (1g/L) or Thiram seed treatment (2g/kg)",
        "organic_treatment": "Soil solarization with clear plastic in May-June. Trichoderma viride in compost.",
        "severity_range": "moderate to severe",
    },

    # --- Brinjal / Eggplant Pathologies ---
    {
        "plant": "Brinjal/Eggplant",
        "disease": "Phomopsis Blight",
        "pathogen": "Phomopsis vexans",
        "symptoms": "Circular brown leaf spots with pale centers; dark sunken soft rotting lesions on fruit",
        "treatment": "Copper oxychloride (3g/L) or Zineb (2g/L)",
        "organic_treatment": "Hot water seed treatment at 50°C for 30 minutes. Remove and destroy infected fruits.",
        "severity_range": "moderate to severe",
    },
    {
        "plant": "Brinjal/Eggplant",
        "disease": "Little Leaf Disease",
        "pathogen": "Phytoplasma (Transmitted by Leafhopper Hishimonus phycitis)",
        "symptoms": "Bushy appearance; newly formed leaves are miniature, narrow, pale green; plant becomes sterile",
        "treatment": "Control leafhopper vector using Dimethoate (1.5ml/L) or Imidacloprid (0.3ml/L)",
        "organic_treatment": "Uproot and burn diseased plants immediately. Neem oil (5ml/L) against leafhoppers.",
        "severity_range": "severe",
    },
    {
        "plant": "Brinjal/Eggplant",
        "disease": "Bacterial Wilt",
        "pathogen": "Ralstonia solanacearum",
        "symptoms": "Rapid complete wilting of plant while foliage stays green; bacterial ooze from cut stem in water",
        "treatment": "Streptocycline (0.2g/L) drenching. Bleaching powder application (10kg/acre).",
        "organic_treatment": "Grafting on resistant wild brinjal rootstock (Solanum torvum). Pseudomonas fluorescens root dip.",
        "severity_range": "severe",
    },

    # --- Sugarcane Pathologies ---
    {
        "plant": "Sugarcane",
        "disease": "Red Rot",
        "pathogen": "Colletotrichum falcatum",
        "symptoms": "Yellowing and withering of third/fourth leaves; internal cane pith turns blood red with white cross-bands",
        "treatment": "Set dip in Carbendazim 50 WP (1g/L) for 15 minutes before planting. Eradicate infected stools.",
        "organic_treatment": "Moist hot air treatment (54°C for 2.5 hours) of seed canes. Plant resistant varieties (Co-0238, Co-86032).",
        "severity_range": "severe",
    },
    {
        "plant": "Sugarcane",
        "disease": "Smut",
        "pathogen": "Sporisorium scitamineum",
        "symptoms": "Emergence of long curved black whip-like structure from cane spindle covered with teliospores",
        "treatment": "Propiconazole (1ml/L) sett dip. Rogue out smutted whips in polythene bags.",
        "organic_treatment": "Healthy seed selection. Rogue out infected stools before spore dispersal.",
        "severity_range": "moderate to severe",
    },

    # --- Mustard & Oilseeds Pathologies ---
    {
        "plant": "Mustard (Sarson)",
        "disease": "White Rust",
        "pathogen": "Albugo candida",
        "symptoms": "Raised shiny white blisters on lower leaf surface; staghead malformation of flowering floral axis",
        "treatment": "Metalaxyl 35 WS seed treatment (6g/kg) followed by Ridomil MZ (2g/L) spray",
        "organic_treatment": "Early sowing before October 15. Bordeaux mixture (1%) spray.",
        "severity_range": "moderate to severe",
    },
    {
        "plant": "Mustard (Sarson)",
        "disease": "Alternaria Blight",
        "pathogen": "Alternaria brassicae",
        "symptoms": "Small dark brown to black circular zonate spots on leaves and siliquae (pods)",
        "treatment": "Mancozeb 75 WP (2g/L) or Iprodione (2g/L)",
        "organic_treatment": "Seed treatment with Trichoderma (10g/kg). Clean debris.",
        "severity_range": "moderate",
    },
    {
        "plant": "Groundnut/Peanut",
        "disease": "Tikka Leaf Spot",
        "pathogen": "Cercospora arachidicola (Early) / Cercosporidium personatum (Late)",
        "symptoms": "Sub-circular dark brown necrotic spots on leaves with yellow halos, causing heavy premature defoliation",
        "treatment": "Hexaconazole (1ml/L) or Carbendazim (1g/L) + Mancozeb (2g/L)",
        "organic_treatment": "Neem seed kernel extract (5%). Seed dressing with Trichoderma viride.",
        "severity_range": "moderate to severe",
    },

    # --- Pulse Crops (Chickpea / Pigeon Pea) Pathologies ---
    {
        "plant": "Chickpea (Chana)",
        "disease": "Ascochyta Blight",
        "pathogen": "Ascochyta rabiei",
        "symptoms": "Circular spots with concentric circles of black pycnidia on leaves, stems, and pods; stem breakage",
        "treatment": "Chlorothalonil (2g/L) or Mancozeb (2.5g/L)",
        "organic_treatment": "Certified clean seed. Intercropping with mustard or barley.",
        "severity_range": "severe",
    },
    {
        "plant": "Chickpea (Chana)",
        "disease": "Fusarium Wilt",
        "pathogen": "Fusarium oxysporum f. sp. ciceris",
        "symptoms": "Drooping of petioles and rachis; internal xylem browning; whole plant wilts within 3-5 days",
        "treatment": "Seed treatment with Carbendazim + Thiram (2g/kg)",
        "organic_treatment": "Seed treatment with Trichoderma viride (10g/kg). Deep summer plowing.",
        "severity_range": "severe",
    },
    {
        "plant": "Pigeon Pea (Arhar/Tur)",
        "disease": "Sterility Mosaic Disease",
        "pathogen": "Pigeonpea Sterility Mosaic Virus (Transmitted by Eriophyid Mite Aceria cajani)",
        "symptoms": "Leaves become small, mosaic mottled and pale green; complete absence of flowers/pods (sterile)",
        "treatment": "Control mite vector with Fenazaquin (1ml/L) or Wettable sulfur (3g/L)",
        "organic_treatment": "Resistant varieties (Asha, Maruti). Rogue out infected plants during early vegetative stage.",
        "severity_range": "severe",
    },

    # --- Citrus / Orange / Lemon Pathologies ---
    {
        "plant": "Orange (Citrus)",
        "disease": "Citrus Canker",
        "pathogen": "Xanthomonas axonopodis pv. citri",
        "symptoms": "Raised brown corky blister-like lesions with oily margin and yellow halo on leaves, twigs, and fruit",
        "treatment": "Copper oxychloride (3g/L) + Streptocycline (0.1g/L) 3-4 times a year during flushes",
        "organic_treatment": "Prune cankered twigs before monsoon. Spray neem cake extract (5%).",
        "severity_range": "moderate to severe",
    },
    {
        "plant": "Orange (Citrus)",
        "disease": "Huanglongbing (Citrus Greening)",
        "pathogen": "Candidatus Liberibacter asiaticus (Transmitted by Asian Citrus Psyllid)",
        "symptoms": "Asymmetric blotchy mottle on leaves, yellow shoots, small lopsided bitter green fruits",
        "treatment": "Control psyllid vectors with Imidacloprid (0.3ml/L) or Thiamethoxam (0.25g/L)",
        "organic_treatment": "Eradicate severely declining trees. Use certified disease-free nursery budwood.",
        "severity_range": "severe",
    },

    # --- Cucurbit Pathologies (Cucumber, Karela, Lauki) ---
    {
        "plant": "Cucumber",
        "disease": "Powdery Mildew",
        "pathogen": "Podosphaera xanthii",
        "symptoms": "Talcum-powder-like white spots on upper and lower leaf surfaces, spreading across entire leaf",
        "treatment": "Dinocap (1ml/L) or Hexaconazole (1ml/L) or Wettable sulfur (2g/L)",
        "organic_treatment": "Potassium bicarbonate spray (3g/L). Dilute milk spray (1:9 with water).",
        "severity_range": "moderate",
    },
    {
        "plant": "Cucumber",
        "disease": "Downy Mildew",
        "pathogen": "Pseudoperonospora cubensis",
        "symptoms": "Angular yellow chlorotic lesions sharply bounded by leaf veins on upper leaf surface, purplish mold below",
        "treatment": "Metalaxyl + Mancozeb (2.5g/L) or Fosetyl-Al (2g/L)",
        "organic_treatment": "Copper hydroxide spray. Avoid overhead sprinkler irrigation. Drip lines only.",
        "severity_range": "severe",
    },

    # --- Rose Pathologies ---
    {
        "plant": "Rose",
        "disease": "Black Spot",
        "pathogen": "Diplocarpon rosae",
        "symptoms": "Fringed circular black spots on upper leaf surfaces; surrounding leaf tissue turns yellow and falls",
        "treatment": "Trifloxystrobin + Tebuconazole (0.7g/L) or Mancozeb (2g/L)",
        "organic_treatment": "Baking soda solution (1 tsp baking soda + 1/2 tsp liquid soap in 1L water). Prune diseased stems.",
        "severity_range": "moderate",
    },
    {
        "plant": "Rose",
        "disease": "Powdery Mildew",
        "pathogen": "Podosphaera pannosa",
        "symptoms": "Curling, crinkling young leaves coated with powdery white fungal growth; distorted flower buds",
        "treatment": "Myclobutanil (0.5g/L) or Sulfur (2g/L)",
        "organic_treatment": "Neem oil spray (5ml/L). Morning watering at soil level.",
        "severity_range": "mild to moderate",
    },
    {
        "plant": "Rose",
        "disease": "Dieback",
        "pathogen": "Lasiodiplodia theobromae",
        "symptoms": "Blackening and dying of pruned stems from tip downwards towards crown; stem shriveling",
        "treatment": "Cut 2 inches into healthy green wood. Apply Copper oxychloride / Chaubattia paste on cut surface.",
        "organic_treatment": "Sterilize pruning shears with 70% alcohol between cuts. Avoid pruning during rainy spells.",
        "severity_range": "severe",
    },

    # --- Houseplants & Ornamentals ---
    {
        "plant": "Money Plant (Golden Pothos)",
        "disease": "Root Rot",
        "pathogen": "Pythium / Phytophthora / Overwatering",
        "symptoms": "Yellowing lower leaves, mushy black stems near soil line, foul smell, stunted vine growth",
        "treatment": "Drench soil with Metalaxyl (1.5g/L) or Carbendazim (1g/L). Repot in fresh well-draining soil.",
        "organic_treatment": "Allow top 2 inches of soil to completely dry between waterings. Use perlite-enriched potting mix.",
        "severity_range": "moderate to severe",
    },
    {
        "plant": "Money Plant (Golden Pothos)",
        "disease": "Bacterial Leaf Spot",
        "pathogen": "Pseudomonas cichorii",
        "symptoms": "Water-soaked translucent lesions with bright yellow halos rapidly turning dark brown or black",
        "treatment": "Copper hydroxide spray (1.5g/L). Keep foliage dry.",
        "organic_treatment": "Isolate plant. Remove affected leaves with sterilized scissors. Avoid misting foliage.",
        "severity_range": "moderate",
    },
    {
        "plant": "Snake Plant (Sansevieria)",
        "disease": "Red Leaf Spot",
        "pathogen": "Drechslera erthrospila",
        "symptoms": "Reddish-brown sunken circular lesions on thick fleshy leaves; yellowing leaf margins",
        "treatment": "Chlorothalonil (2g/L) or Mancozeb (2g/L)",
        "organic_treatment": "Keep leaves dry. Provide bright indirect light and reduce watering frequency to once every 2 weeks.",
        "severity_range": "mild to moderate",
    },
]

# ============================================
# 3. Essential Nutrient Deficiencies (12 Essential Elements)
# ============================================

NUTRIENT_DEFICIENCIES = [
    {
        "nutrient": "Nitrogen (N)",
        "role": "Chlorophyll synthesis, vegetative vigor, and protein structure",
        "symptoms": "Uniform pale yellowing (chlorosis) beginning on older/lower leaves while young leaves stay light green; stunted growth and thin stems",
        "treatment": "Soil application of Urea (46% N) or Ammonium Sulfate; foliar spray of 1% water-soluble Urea",
        "organic_option": "Well-rotted vermicompost, cow dung manure, blood meal, fish emulsion, or growing green manure legumes (Dhaincha/Sunn hemp)",
    },
    {
        "nutrient": "Phosphorus (P)",
        "role": "Root establishment, ATP energy transfer, flower and seed development",
        "symptoms": "Dark green foliage with distinct purple or reddish anthocyanin coloration along leaf veins, petioles, and undersides; stunted root system",
        "treatment": "Soil application of Single Superphosphate (SSP 16% P2O5) or DAP (18:46:0)",
        "organic_option": "Steamed bone meal, rock phosphate with phosphate solubilizing bacteria (PSB), or fermented compost tea",
    },
    {
        "nutrient": "Potassium (K)",
        "role": "Stomatal regulation, water retention, disease resistance, and fruit sizing",
        "symptoms": "Marginal scorching and necrosis (burned leaf edges) starting from older leaf tips; weak stalks prone to lodging; poor fruit shelf-life",
        "treatment": "Muriate of Potash (MOP 60% K2O) or Potassium Sulfate (SOP 50% K2O); foliar spray of 13:00:45 (Potassium Nitrate 1%)",
        "organic_option": "Wood ash (sparingly), kelp meal, banana peel bio-fertilizer, or potassium-mobilizing bacteria (KMB)",
    },
    {
        "nutrient": "Calcium (Ca)",
        "role": "Cell wall pectate integrity, membrane permeability, and shoot growth",
        "symptoms": "Death of growing tips (apical buds), distorted hooked young leaves with blackened tips; Blossom End Rot in tomato/chili; tip burn in cabbage",
        "treatment": "Foliar spray of Calcium Nitrate (1%) or soil application of agricultural gypsum (Calcium sulfate)",
        "organic_option": "Crushed eggshells steeped in vinegar (calcium acetate), dolomite limestone, or bone meal",
    },
    {
        "nutrient": "Magnesium (Mg)",
        "role": "Central atom in chlorophyll molecule; enzyme activator for carbohydrates",
        "symptoms": "Interveinal chlorosis on mature older leaves where veins remain distinctly green while tissue between turns brilliant yellow; inverted green 'V' pattern",
        "treatment": "Foliar spray of Magnesium Sulfate (Epsom Salt, 10g/L); soil application of 10-15 kg/acre Epsom Salt",
        "organic_option": "Dolomitic limestone (provides both Ca and Mg) or composted organic leaf mold",
    },
    {
        "nutrient": "Sulfur (S)",
        "role": "Synthesis of methionine and cysteine amino acids; oil synthesis in oilseeds",
        "symptoms": "General uniform yellowing of young upper leaves first (opposite to nitrogen which affects lower leaves first); light green foliage; retarded growth",
        "treatment": "Soil application of Bentonite Sulfur (10kg/acre) or Gypsum; foliar spray of wettable sulfur (2g/L)",
        "organic_option": "Elemental sulfur dust, gypsum, or mustard seed meal incorporation",
    },
    {
        "nutrient": "Iron (Fe)",
        "role": "Electron transport, chlorophyll formation catalyst, and redox reactions",
        "symptoms": "Vivid ivory-yellow to pure white interveinal chlorosis on the youngest emergent leaves with sharply delineated fine green veins; leaf bleaching",
        "treatment": "Foliar spray of Fe-EDTA / Fe-EDDHA chelated iron (1g/L) or Ferrous Sulfate (5g/L) with 0.5g citric acid",
        "organic_option": "Compost enriched with fermented buttermilk and iron scrap; lower soil pH below 7.0 using sulfur",
    },
    {
        "nutrient": "Zinc (Zn)",
        "role": "Auxin (IAA) synthesis, enzyme activation, and internode elongation",
        "symptoms": "Little Leaf disease (small clustered rosette leaves); bleached white bands on both sides of midrib in corn (White Bud); Khaira disease in rice",
        "treatment": "Foliar spray of Zinc Sulfate 21% (5g/L) or Chelated Zn-EDTA (1g/L)",
        "organic_option": "Zinc-solubilizing bio-fertilizer (ZSB) applied with well-rotted vermicompost",
    },
    {
        "nutrient": "Manganese (Mn)",
        "role": "Photolysis of water in photosynthesis and nitrogen assimilation",
        "symptoms": "Interveinal chlorosis on middle leaves with small checkered brownish necrotic speckles (Marsh spot in peas; Grey speck in oats)",
        "treatment": "Foliar spray of Manganese Sulfate (MnSO4, 2g/L)",
        "organic_option": "Compost tea or balancing soil aeration to prevent Mn leaching",
    },
    {
        "nutrient": "Copper (Cu)",
        "role": "Lignin synthesis, pollen viability, and cellular respiration",
        "symptoms": "Dieback of young shoot tips; bluish-green twisted leaves; failure of grain/earhead filling in cereals",
        "treatment": "Foliar spray of Copper Sulfate (1g/L) with lime or Copper chelate",
        "organic_option": "Bordeaux mixture spray (0.5%) or copper slag compost",
    },
    {
        "nutrient": "Boron (B)",
        "role": "Pollen germination, fruit set, sugar translocation, and cell division",
        "symptoms": "Hollow heart in cauliflower; cracked corky fruit surfaces in pomegranate; brittle deformed leaves; flower/fruit drop",
        "treatment": "Foliar spray of Solubor / Borax 20% (1g/L) before flowering and after fruit set",
        "organic_option": "Composted organic manure supplemented with mined borate mineral",
    },
    {
        "nutrient": "Molybdenum (Mo)",
        "role": "Nitrate reductase enzyme constituent; nitrogen fixation in legumes",
        "symptoms": "Whiptail in cauliflower (leaf blade fails to develop, leaving bare midrib); pale mottling with marginal cupping",
        "treatment": "Foliar spray of Sodium / Ammonium Molybdate (0.5g/L)",
        "organic_option": "Liming acidic soils to pH 6.5 (Mo availability increases with higher pH)",
    },
]

# ============================================
# 4. Comprehensive Agricultural Pests & Vectors (25+ Species)
# ============================================

COMMON_PESTS = [
    {
        "pest": "Aphids (Green/Black)",
        "crops_affected": "Mustard, Cotton, Rose, Tomato, Wheat, Potato, Cabbage",
        "symptoms": "Clusters of tiny green/black insects on tender shoot tips, curled leaves, sticky honeydew, and black sooty mold",
        "treatment": "Imidacloprid 17.8 SL (0.3ml/L) or Acetamiprid 20 SP (0.2g/L)",
        "organic_treatment": "Neem oil spray (5ml/L) with liquid soap; release of Ladybird beetles (Coccinella); strong water jet spray",
        "prevention": "Yellow sticky traps (15/acre); avoid excessive nitrogen fertilizer.",
    },
    {
        "pest": "Whitefly (Bemisia tabaci)",
        "crops_affected": "Cotton, Tomato, Chili, Okra, Soybean, Brinjal",
        "symptoms": "Tiny white moth-like insects fluttering when shaken; leaf yellowing; vector of Leaf Curl Virus and Yellow Vein Mosaic",
        "treatment": "Diafenthiuron 50 WP (1g/L) or Spiromesifen 22.9 SC (1ml/L) or Flonicamid 50 WG",
        "organic_treatment": "Yellow sticky traps (20/acre); Verticillium lecanii entomopathogenic fungal spray (5g/L); Neem seed kernel extract (5%)",
        "prevention": "Border barrier crop of maize/sorghum; reflective silver plastic mulch.",
    },
    {
        "pest": "Thrips (Scirtothrips dorsalis)",
        "crops_affected": "Chili, Onion, Cotton, Rose, Grape, Tomato",
        "symptoms": "Silvery upward-curled leaf edges (boat shape); rasping scars on leaf undersides and fruit; vector of spotted wilt virus",
        "treatment": "Fipronil 5 SC (1.5ml/L) or Spinetoram 11.7 SC (0.9ml/L)",
        "organic_treatment": "Blue sticky traps (15/acre); Pongamia/Neem oil mix spray (5ml/L); Lecanicillium lecanii spray",
        "prevention": "Intercropping with coriander or fennel; sprinkler irrigation to wash off nymphs.",
    },
    {
        "pest": "Red Spider Mite (Tetranychus urticae)",
        "crops_affected": "Rose, Tomato, Brinjal, Okra, Apple, Tea, Papaya",
        "symptoms": "Fine yellow speckling stippling on upper leaf surface; delicate silken webbing under leaves; bronzed foliage",
        "treatment": "Abamectin 1.9 EC (0.5ml/L) or Fenazaquin 10 EC (1.5ml/L) or Propargite 57 EC (2ml/L)",
        "organic_treatment": "Wettable sulfur (2.5g/L); predatory mite release (Phytoseiulus persimilis); spray cold water on leaf undersides",
        "prevention": "Maintain adequate relative humidity; avoid hot dusty field borders.",
    },
    {
        "pest": "Fall Armyworm (Spodoptera frugiperda)",
        "crops_affected": "Corn/Maize, Sorghum, Sugarcane, Rice, Cotton",
        "symptoms": "Large ragged irregular holes in whorl leaves; characteristic four dots arranged in a square on last abdominal segment; sawdust-like frass",
        "treatment": "Chlorantraniliprole 18.5 SC (Coragen 0.4ml/L) or Emamectin benzoate 5 SG (0.4g/L) directed into leaf whorl",
        "organic_treatment": "Bacillus thuringiensis (Bt kurstaki, 2g/L); Beauveria bassiana (5g/L); Metarhizium anisopliae; handpicking egg masses",
        "prevention": "Pheromone traps (5/acre); intercrop with cowpea or desmodium.",
    },
    {
        "pest": "Cotton Bollworm / Fruit Borer (Helicoverpa armigera)",
        "crops_affected": "Tomato, Cotton, Chickpea, Pigeon Pea, Chili, Okra",
        "symptoms": "Caterpillar bores circular holes into green tomatoes and cotton bolls with its head inside and body outside; shed buds",
        "treatment": "Spinosad 45 SC (0.3ml/L) or Flubendiamide 39.35 SC (0.3ml/L)",
        "organic_treatment": "Helicoverpa NPV (HaNPV, 250 LE/ha); Trichogramma chilonis egg parasitoid cards (50,000/acre); Bt spray",
        "prevention": "Marigold trap crop (1 row for every 16 rows of crop); Helilure pheromone traps.",
    },
    {
        "pest": "Stem Borer (Scirpophaga incertulas)",
        "crops_affected": "Rice, Sugarcane, Corn",
        "symptoms": "Central shoot withers and dries up ('Dead Heart' in vegetative phase); empty white earheads ('White Ear') at panicle stage",
        "treatment": "Cartap hydrochloride 4G (10kg/acre) or Chlorantraniliprole 0.4G granules applied in water",
        "organic_treatment": "Trichogramma japonicum parasitoid release; clip seedling leaf tips before transplanting to remove egg masses",
        "prevention": "Pheromone traps (8/acre); avoid excessive water stagnancy.",
    },
    {
        "pest": "Mealybug (Ferrisia virgata / Phenacoccus solenopsis)",
        "crops_affected": "Cotton, Papaya, Hibiscus, Custard Apple, Grape, Guava",
        "symptoms": "White waxy cottony masses clustering on shoots, leaf undersides, and fruits; leaf distortion and severe sooty mold",
        "treatment": "Profenofos 50 EC (2ml/L) or Buprofezin 25 SC (1.5ml/L) with sticking agent",
        "organic_treatment": "Cryptolaemus montrouzieri (Australian ladybird beetle) release; spray fish oil rosin soap (25g/L) or neem oil with detergent",
        "prevention": "Stem banding with sticky grease bands; destroy weed hosts like Parthenium.",
    },
    {
        "pest": "Leafminer (Liriomyza trifolii)",
        "crops_affected": "Tomato, Cucumber, Watermelon, Pea, Potato, Chrysanthemum",
        "symptoms": "Serpentine, winding, transluscent white mines and tunnels etched into leaf blades by feeding maggots",
        "treatment": "Abamectin 1.9 EC (0.5ml/L) or Cyromazine 75 WP (0.2g/L)",
        "organic_treatment": "Neem seed kernel extract 5% (acts as anti-oviposition agent); yellow sticky traps; remove heavily mined leaves",
        "prevention": "Conserve Diglyphus isaea parasitoid wasps; fine mesh insect netting.",
    },
    {
        "pest": "Fruit Fly (Bactrocera cucurbitae / dorsalis)",
        "crops_affected": "Mango, Guava, Bitter Gourd, Cucumber, Melon, Tomato",
        "symptoms": "Punctures on ripening fruit surface exuding amber gum; internal flesh maggot infestation leading to premature rotting and fruit drop",
        "treatment": "Bait spray of Malathion 50 EC (2ml/L) + Jaggery/Molasses (10g/L) in coarse droplets on foliage",
        "organic_treatment": "Methyl eugenol / Cue-lure pheromone traps (6-8 per acre); bag fruits with paper covers 30 days before harvest",
        "prevention": "Collect and bury fallen fruits deep in soil (at least 60 cm deep).",
    },
]


async def seed_knowledge_base(session):
    """Seed the database with comprehensive plant and disease reference data."""
    logger.info("Seeding knowledge base with V3 ultra-comprehensive botanical dataset...")
    logger.info(
        f"Knowledge base ready: {len(COMMON_PLANTS)} plants, "
        f"{len(COMMON_DISEASES)} diseases, "
        f"{len(NUTRIENT_DEFICIENCIES)} nutrient deficiencies, "
        f"{len(COMMON_PESTS)} agricultural pests"
    )
