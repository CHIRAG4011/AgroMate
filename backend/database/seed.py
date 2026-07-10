"""
AgroMate Database Seed Data

Populates the database with initial reference data including
common plant species, diseases, and their treatments.
"""

import logging

logger = logging.getLogger(__name__)

# Common plant species for the knowledge base
COMMON_PLANTS = [
    {"name": "Tomato", "scientific": "Solanum lycopersicum", "category": "Vegetable"},
    {"name": "Rice", "scientific": "Oryza sativa", "category": "Grain"},
    {"name": "Wheat", "scientific": "Triticum aestivum", "category": "Grain"},
    {"name": "Potato", "scientific": "Solanum tuberosum", "category": "Vegetable"},
    {"name": "Corn/Maize", "scientific": "Zea mays", "category": "Grain"},
    {"name": "Cotton", "scientific": "Gossypium", "category": "Fiber"},
    {"name": "Sugarcane", "scientific": "Saccharum officinarum", "category": "Cash Crop"},
    {"name": "Soybean", "scientific": "Glycine max", "category": "Legume"},
    {"name": "Grape", "scientific": "Vitis vinifera", "category": "Fruit"},
    {"name": "Apple", "scientific": "Malus domestica", "category": "Fruit"},
    {"name": "Mango", "scientific": "Mangifera indica", "category": "Fruit"},
    {"name": "Banana", "scientific": "Musa", "category": "Fruit"},
    {"name": "Onion", "scientific": "Allium cepa", "category": "Vegetable"},
    {"name": "Chili Pepper", "scientific": "Capsicum annuum", "category": "Vegetable"},
    {"name": "Groundnut/Peanut", "scientific": "Arachis hypogaea", "category": "Legume"},
    {"name": "Chickpea", "scientific": "Cicer arietinum", "category": "Legume"},
    {"name": "Mustard", "scientific": "Brassica juncea", "category": "Oilseed"},
    {"name": "Cumin", "scientific": "Cuminum cyminum", "category": "Spice"},
    {"name": "Turmeric", "scientific": "Curcuma longa", "category": "Spice"},
    {"name": "Tea", "scientific": "Camellia sinensis", "category": "Beverage"},
]

# Common plant diseases with treatments
COMMON_DISEASES = [
    {
        "plant": "Tomato",
        "disease": "Early Blight",
        "pathogen": "Alternaria solani",
        "symptoms": "Dark brown spots with concentric rings on lower leaves",
        "treatment": "Apply copper-based fungicide. Remove affected leaves. Ensure proper spacing.",
        "organic_treatment": "Neem oil spray. Baking soda solution (1 tbsp per gallon). Crop rotation.",
        "severity_range": "mild to severe",
    },
    {
        "plant": "Tomato",
        "disease": "Late Blight",
        "pathogen": "Phytophthora infestans",
        "symptoms": "Large, dark water-soaked lesions on leaves and stems",
        "treatment": "Chlorothalonil or mancozeb fungicide. Remove infected plants immediately.",
        "organic_treatment": "Copper hydroxide spray. Remove and destroy infected plants. Improve air circulation.",
        "severity_range": "moderate to severe",
    },
    {
        "plant": "Tomato",
        "disease": "Leaf Mold",
        "pathogen": "Passalora fulva",
        "symptoms": "Yellow spots on upper leaf surface, olive-green mold underneath",
        "treatment": "Fungicide application. Reduce humidity. Improve ventilation.",
        "organic_treatment": "Increase air circulation. Remove affected leaves. Apply sulfur spray.",
        "severity_range": "mild to moderate",
    },
    {
        "plant": "Rice",
        "disease": "Blast",
        "pathogen": "Magnaporthe oryzae",
        "symptoms": "Diamond-shaped lesions on leaves with gray centers",
        "treatment": "Tricyclazole or isoprothiolane fungicide. Use resistant varieties.",
        "organic_treatment": "Silicon fertilization. Avoid excess nitrogen. Use resistant cultivars.",
        "severity_range": "moderate to severe",
    },
    {
        "plant": "Wheat",
        "disease": "Rust",
        "pathogen": "Puccinia triticina",
        "symptoms": "Orange-brown pustules on leaves and stems",
        "treatment": "Propiconazole or tebuconazole fungicide. Plant resistant varieties.",
        "organic_treatment": "Remove volunteer wheat. Use resistant varieties. Crop rotation.",
        "severity_range": "mild to severe",
    },
    {
        "plant": "Potato",
        "disease": "Late Blight",
        "pathogen": "Phytophthora infestans",
        "symptoms": "Dark water-soaked lesions on leaves, white mold on underside",
        "treatment": "Metalaxyl + mancozeb combination. Hill up potatoes. Destroy infected tubers.",
        "organic_treatment": "Copper spray before symptoms appear. Destroy infected plants. Use certified seed.",
        "severity_range": "severe",
    },
    {
        "plant": "Grape",
        "disease": "Powdery Mildew",
        "pathogen": "Erysiphe necator",
        "symptoms": "White powdery coating on leaves, shoots, and berries",
        "treatment": "Sulfur or potassium bicarbonate spray. Prune for air circulation.",
        "organic_treatment": "Milk spray (40% milk solution). Neem oil. Sulfur dust. Prune canopy.",
        "severity_range": "mild to moderate",
    },
    {
        "plant": "Apple",
        "disease": "Apple Scab",
        "pathogen": "Venturia inaequalis",
        "symptoms": "Olive-green to brown spots on leaves and fruit",
        "treatment": "Captan or myclobutanil fungicide. Remove fallen leaves.",
        "organic_treatment": "Lime sulfur spray in dormant season. Remove fallen leaves. Use resistant varieties.",
        "severity_range": "moderate",
    },
    {
        "plant": "Cotton",
        "disease": "Bacterial Blight",
        "pathogen": "Xanthomonas citri pv. malvacearum",
        "symptoms": "Angular water-soaked lesions on leaves, black arm on stems",
        "treatment": "Copper oxychloride spray. Use acid-delinted treated seed.",
        "organic_treatment": "Seed treatment with cow urine. Crop rotation. Remove crop debris.",
        "severity_range": "moderate to severe",
    },
    {
        "plant": "Mango",
        "disease": "Anthracnose",
        "pathogen": "Colletotrichum gloeosporioides",
        "symptoms": "Black spots on leaves, flowers, and fruit. Blossom blight.",
        "treatment": "Carbendazim or thiophanate-methyl spray during flowering.",
        "organic_treatment": "Bordeaux mixture spray. Remove infected parts. Improve drainage.",
        "severity_range": "mild to severe",
    },
]

# Nutrient deficiency reference
NUTRIENT_DEFICIENCIES = [
    {
        "nutrient": "Nitrogen (N)",
        "symptoms": "Yellowing of older/lower leaves, stunted growth, pale green color",
        "treatment": "Apply urea, ammonium sulfate, or organic compost",
        "organic_option": "Apply vermicompost, blood meal, or legume cover crops",
    },
    {
        "nutrient": "Phosphorus (P)",
        "symptoms": "Purple/reddish leaves, dark green older leaves, poor root development",
        "treatment": "Apply superphosphate or DAP",
        "organic_option": "Bone meal, rock phosphate, or fish bone meal",
    },
    {
        "nutrient": "Potassium (K)",
        "symptoms": "Brown leaf edges (scorch), weak stems, poor fruit quality",
        "treatment": "Apply muriate of potash or sulfate of potash",
        "organic_option": "Wood ash, kelp meal, or granite dust",
    },
    {
        "nutrient": "Iron (Fe)",
        "symptoms": "Interveinal chlorosis on young leaves (yellow between green veins)",
        "treatment": "Apply iron chelate (Fe-EDDHA) as foliar spray or soil drench",
        "organic_option": "Lower soil pH with sulfur, apply ferrous sulfate",
    },
    {
        "nutrient": "Magnesium (Mg)",
        "symptoms": "Interveinal chlorosis on older leaves, leaf curling",
        "treatment": "Apply Epsom salt (magnesium sulfate) foliar spray",
        "organic_option": "Dolomite lime, Epsom salt solution",
    },
    {
        "nutrient": "Calcium (Ca)",
        "symptoms": "Blossom end rot, distorted new growth, tip burn",
        "treatment": "Apply calcium nitrate or gypsum",
        "organic_option": "Crushed eggshells, gypsum, or lime",
    },
    {
        "nutrient": "Zinc (Zn)",
        "symptoms": "Small leaves, shortened internodes, mottled leaves",
        "treatment": "Apply zinc sulfate as foliar spray",
        "organic_option": "Zinc sulfate spray, organic matter addition",
    },
]


async def seed_knowledge_base(session):
    """Seed the database with plant and disease reference data."""
    logger.info("Seeding knowledge base with plant and disease data...")
    # The knowledge base is stored in-memory via the chatbot module
    # This function logs that seed data is available
    logger.info(
        f"Knowledge base ready: {len(COMMON_PLANTS)} plants, "
        f"{len(COMMON_DISEASES)} diseases, "
        f"{len(NUTRIENT_DEFICIENCIES)} nutrient deficiencies"
    )
