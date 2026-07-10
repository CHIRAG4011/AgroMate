/* ============================================================
   AgroMate Knowledge Base — V2 [NEW]
   Offline farm handbook: crop guides, disease encyclopedia,
   and organic bio-pesticide tutorials.
   ============================================================ */

let activeKnowledgeCategory = 'crops';
let knowledgeSearchQuery = '';

const KNOWLEDGE_HANDBOOK = {
  crops: [
    {
      title: "Rice Cultivation (Kharif)",
      desc: "Optimal temperature: 22-32°C. Requires standing water (clayey loam soil). Sow nursery in June, transplant in July. Yield cycle: 120-150 days.",
      details: "NPK ratio: 120:60:40 kg/ha. Keep field flooded with 2-5cm water during tillering and flowering stage. Susceptible to blast disease and stem borers."
    },
    {
      title: "Wheat Farming (Rabi)",
      desc: "Optimal temperature: 15-25°C. Requires loam soil. Sow in November, harvest in March/April. Yield cycle: 110-140 days.",
      details: "Requires 4-6 irrigations at critical growth periods (Crown Root Initiation stage is most critical). Susceptible to rust diseases."
    },
    {
      title: "Tomato Management",
      desc: "Optimal temperature: 20-28°C. Sandy loam soil, pH 6.0-7.0. Can be sown year-round. Yield cycle: 80-110 days.",
      details: "Water at base to keep foliage dry, preventing fungal blights. Stake plants to support fruit loads. Feed with potassium-rich organic mulch during fruit set."
    }
  ],
  diseases: [
    {
      title: "Early Blight (Alternaria solani)",
      desc: "Fungal disease targeting tomatoes and potatoes. Symptoms: Concentric dark target-like spots starting on older lower leaves.",
      details: "Organic Remedy: Spray 0.5% neem oil solution or baking soda mix (3g per liter). Prune lower leaves to enhance airflow and isolate infected plants."
    },
    {
      title: "Common Rust (Puccinia sorghi)",
      desc: "Fungal pathogen attacking corn leaves. Symptoms: Elongated golden-brown pustules on both upper and lower leaf surfaces.",
      details: "Organic Remedy: Apply sulfur-based organic dust or copper sulfate solution. Plant rust-resistant seed hybrids."
    },
    {
      title: "Bacterial Spot (Xanthomonas)",
      desc: "Bacterial infection causing lesions on peppers and tomatoes. Symptoms: Small water-soaked spots on foliage turning dark brown.",
      details: "Organic Remedy: Spray copper fungicide early in the season. Avoid overhead sprinkler irrigation. Remove crop residues after harvest."
    }
  ],
  organic: [
    {
      title: "Neem Oil Bio-Pesticide",
      desc: "Natural repellent active against aphids, spider mites, whiteflies, and fungal powdery mildew.",
      details: "Preparation: Mix 5ml pure cold-pressed neem oil with 2ml organic liquid soap inside 1 liter of warm water. Spray thoroughly under leaf surfaces every 7 days."
    },
    {
      title: "Garlic-Chili Insecticide Spray",
      desc: "Strong contact insecticide targeting caterpillars, leaf miners, and chewing beetles.",
      details: "Preparation: Grind 2 heads of garlic and 5 hot green chilies into a paste. Steep in 1 liter water for 24 hours. Strain liquid, dilute 1:5 with water, and spray."
    },
    {
      title: "Buttermilk Fungal Spray",
      desc: "Sour buttermilk spray creates an acidic film that prevents powdery mildew and leaf blights.",
      details: "Preparation: Dilute sour buttermilk or curd with water in 1:10 ratio. Spray on crop foliage during dry mornings."
    }
  ]
};

function renderKnowledge(container) {
  container.innerHTML = `
    <div class="page-header">
      <h1 data-i18n="nav_knowledge">📚 Knowledge Base</h1>
      <p style="color:var(--text-tertiary);">Offline agricultural encyclopedia, guides, and natural farming remedies</p>
    </div>

    <!-- Search Box -->
    <div class="knowledge-search" style="margin-bottom:var(--space-lg);">
      <input type="text" class="input" id="kbSearchInput" placeholder="Search guides, diseases, or organic remedies..." style="padding-left:40px; height:48px;" oninput="filterKbSearch(this.value)">
      <span class="knowledge-search-icon" style="left:12px;">🔍</span>
    </div>

    <!-- Category selector -->
    <div style="display:flex; gap:10px; margin-bottom:var(--space-lg);">
      <button class="knowledge-category ${activeKnowledgeCategory === 'crops' ? 'active' : ''}" onclick="setKbCategory('crops')">
        🌾 Crop Guides
      </button>
      <button class="knowledge-category ${activeKnowledgeCategory === 'diseases' ? 'active' : ''}" onclick="setKbCategory('diseases')">
        🦠 Disease Catalog
      </button>
      <button class="knowledge-category ${activeKnowledgeCategory === 'organic' ? 'active' : ''}" onclick="setKbCategory('organic')">
        🧪 Bio-Pesticides
      </button>
    </div>

    <!-- Handbooks List -->
    <div class="grid-3" id="kbGrid">
      <!-- Loaded dynamically -->
    </div>
  `;

  renderKbCards();
  i18n.translateDOM();
}

function renderKbCards() {
  const grid = document.getElementById('kbGrid');
  if (!grid) return;

  const dataset = KNOWLEDGE_HANDBOOK[activeKnowledgeCategory] || [];
  
  // Filter by search query
  const query = knowledgeSearchQuery.toLowerCase().trim();
  const filtered = dataset.filter(item => 
    item.title.toLowerCase().includes(query) || 
    item.desc.toLowerCase().includes(query) || 
    item.details.toLowerCase().includes(query)
  );

  if (filtered.length === 0) {
    grid.innerHTML = `
      <div style="grid-column: span 3; padding:var(--space-2xl) 0; color:var(--text-tertiary); text-align:center;">
        <span style="font-size:2.5rem; display:block;">📂</span>
        <h3>No matching guides found</h3>
        <p style="font-size:0.8rem; margin-top:4px;">Try modifying your search criteria</p>
      </div>
    `;
    return;
  }

  grid.innerHTML = filtered.map(item => `
    <div class="card" style="text-align:left; display:flex; flex-direction:column; justify-content:space-between;">
      <div>
        <h3 style="color:var(--accent-secondary); font-size:1.15rem; font-weight:700;">${item.title}</h3>
        <p style="font-size:0.85rem; margin-top:8px; line-height:1.5;">${item.desc}</p>
        <p style="font-size:0.8rem; color:var(--text-tertiary); margin-top:12px; border-top:1px solid var(--border-subtle); padding-top:10px; line-height:1.5;">
          ${item.details}
        </p>
      </div>
    </div>
  `).join('');
}

function setKbCategory(cat) {
  activeKnowledgeCategory = cat;
  renderKbCards();
  
  // Update categories highlights
  document.querySelectorAll('.knowledge-category').forEach((btn, index) => {
    const list = ['crops', 'diseases', 'organic'];
    btn.classList.toggle('active', list[index] === cat);
  });
}

function filterKbSearch(val) {
  knowledgeSearchQuery = val;
  renderKbCards();
}
