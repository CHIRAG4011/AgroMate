/* ============================================================
   AgroMate AI Chat Page — V2
   Multilingual, two-tier (summary/detailed) responses,
   integrated voice recorders & dynamic suggestions.
   ============================================================ */

let chatMessages = JSON.parse(localStorage.getItem('agromate-chat-messages') || '[]');
let chatIsLoading = false;

function saveChatMessages() {
  localStorage.setItem('agromate-chat-messages', JSON.stringify(chatMessages.slice(-50)));
}

function renderChat(container) {
  container.innerHTML = `
    <div class="chat-container" style="display:flex; flex-direction:column; height:calc(100vh - 120px); max-width:900px; margin:0 auto;">
      
      <!-- Chat Header -->
      <div style="display:flex; justify-content:space-between; align-items:center; margin-bottom:var(--space-md);">
        <div>
          <h2 data-i18n="nav_chat">💬 AI Chat</h2>
          <p style="font-size:0.8rem; color:var(--text-tertiary);">Conversational agricultural expert assistant</p>
        </div>
        <button class="btn btn-secondary btn-sm" onclick="clearChatHistory()">
          🗑️ Clear
        </button>
      </div>

      <!-- Messages Stream -->
      <div class="chat-messages" id="chatMessages" style="flex:1; overflow-y:auto; padding:var(--space-sm) 0; display:flex; flex-direction:column; gap:var(--space-md);">
        ${chatMessages.length === 0 ? renderChatWelcome() : renderChatList()}
      </div>

      <!-- Quick suggestions -->
      <div id="chatSuggestionsContainer" style="display:flex; gap:6px; flex-wrap:wrap; margin-bottom:var(--space-sm);">
        <!-- Suggestion chips will load dynamically -->
      </div>

      <!-- Typing Indicator -->
      <div class="typing-indicator" id="chatTypingIndicator" style="display: none; margin-bottom:var(--space-sm);">
        <div class="typing-dot"></div>
        <div class="typing-dot"></div>
        <div class="typing-dot"></div>
      </div>

      <!-- Input Area -->
      <div class="chat-input-area" style="position:relative;">
        <div style="display:flex; width:100%; gap:var(--space-sm); align-items:center; background:var(--bg-card); padding:8px; border-radius:var(--radius-xl); border:var(--glass-border);">
          
          <button class="voice-input-btn" id="chatMicBtn" onclick="handleMicClick()" title="Voice Input" style="position:relative; right:0; bottom:0; flex-shrink:0;">
            🎤
          </button>
          
          <textarea
            class="input"
            id="chatInput"
            data-i18n="type_message"
            placeholder="Type a message or speak..."
            rows="1"
            style="border:none; background:transparent; padding:4px 8px; font-size:0.95rem; resize:none; flex:1; outline:none; box-shadow:none; height:24px; min-height:24px; line-height:24px;"
            onkeydown="handleChatKeydown(event)"
          ></textarea>
          
          <button class="btn btn-primary" onclick="sendChatMessage()" style="border-radius:var(--radius-full); width:40px; height:40px; min-height:40px; padding:0; flex-shrink:0;">
            ➤
          </button>
        </div>
      </div>

    </div>
  `;

  // Restore scroll
  const stream = document.getElementById('chatMessages');
  if (stream) stream.scrollTop = stream.scrollHeight;
  
  // Set placeholder translations
  i18n.translateDOM();
}

function renderChatWelcome() {
  const isHi = currentLanguage === 'hi';
  const isGu = currentLanguage === 'gu';
  
  let welcomeTitle = "Hello! I'm AgroMate AI";
  let welcomeDesc = "I can help you with plant health, disease diagnosis, organic remedies, weather warnings, and crop recommendations. Ask me anything!";
  
  if (isHi) {
    welcomeTitle = "नमस्ते! मैं एग्रोमेट एआई हूँ";
    welcomeDesc = "मैं पौधों के स्वास्थ्य, बीमारी की पहचान, जैविक उपचार और फसल सलाह में मदद कर सकता हूँ। कुछ भी पूछें!";
  } else if (isGu) {
    welcomeTitle = "નમસ્તે! હું એગ્રોમેટ એઆઈ છું";
    welcomeDesc = "હું છોડના રોગ નિદાન, જૈવિક ઉપચાર અને પાક સલાહમાં મદદ કરી શકું છું. ગમે તે પૂછો!";
  }

  const suggestionList = [
    { text: "What crops should I plant this season?", label: "🌾 Recommended crops" },
    { text: "How to treat tomato early blight?", label: "🍅 Treat tomato blight" },
    { text: "Tell me about drip irrigation benefits.", label: "💧 Drip irrigation" },
    { text: "Best organic fertilizer for wheat?", label: "🌱 Organic wheat fertilizer" }
  ];

  return `
    <div style="text-align:center; margin:auto 0; padding:var(--space-xl) var(--space-md); color:var(--text-secondary);">
      <span style="font-size:3.5rem; display:block; margin-bottom:12px;">🌱</span>
      <h3 style="font-size:1.4rem; color:var(--text-primary); font-weight:700;">${welcomeTitle}</h3>
      <p style="font-size:0.9rem; margin-top:8px; max-width:500px; margin-left:auto; margin-right:auto; line-height:1.6;">${welcomeDesc}</p>
      
      <div style="display:flex; gap:8px; flex-wrap:wrap; justify-content:center; margin-top:24px; max-width:600px; margin-left:auto; margin-right:auto;">
        ${suggestionList.map(s => `
          <button class="btn btn-secondary btn-sm" onclick="useSuggestion('${s.text.replace(/'/g, "\\'")}')">
            ${s.label}
          </button>
        `).join('')}
      </div>
    </div>
  `;
}

function renderChatList() {
  return chatMessages.map((msg, index) => {
    const isUser = msg.role === 'user';
    const alignClass = isUser ? 'user' : 'assistant';
    const showDetailsButton = !isUser && msg.hasDetailed && msg.response_type === 'summary';
    
    return `
      <div class="chat-bubble ${alignClass}">
        ${!isUser ? `
          <div style="display:flex; justify-content:space-between; align-items:center; margin-bottom:4px; font-size:0.75rem; font-weight:600; color:var(--text-tertiary);">
            <span>🌱 AgroMate AI</span>
            <button class="btn btn-ghost btn-sm" style="padding:0; min-height:auto; font-size:0.9rem;" onclick="speakMessage(${index})">🔊</button>
          </div>
        ` : ''}
        <div>${formatMessage(msg.content)}</div>
        
        ${showDetailsButton ? `
          <div style="margin-top:10px;">
            <button class="btn btn-primary btn-sm" onclick="fetchMoreDetails(${index})">
              📖 <span data-i18n="more_details">More Details</span>
            </button>
          </div>
        ` : ''}
      </div>
    `;
  }).join('');
}

function useSuggestion(text) {
  const input = document.getElementById('chatInput');
  if (input) {
    input.value = text;
    sendChatMessage();
  }
}

function handleChatKeydown(e) {
  if (e.key === 'Enter' && !e.shiftKey) {
    e.preventDefault();
    sendChatMessage();
  }
}

async function sendChatMessage() {
  const input = document.getElementById('chatInput');
  if (!input) return;

  const text = input.value.trim();
  if (!text || chatIsLoading) return;

  ttsPlayer.stop(); // Stop audio if user responds

  // Append user message
  chatMessages.push({ role: 'user', content: text });
  input.value = '';
  saveChatMessages();

  chatIsLoading = true;
  updateChatWindow();

  try {
    const data = await api.post('/chat/', {
      message: text,
      session_id: 'default',
      language: currentLanguage,
      response_type: 'summary'
    });

    chatIsLoading = false;
    
    if (data && data.status === 'success') {
      const resp = data.response;
      
      chatMessages.push({
        role: 'bot',
        content: resp.short_summary || resp.text,
        hasDetailed: resp.has_detailed,
        response_type: resp.response_type
      });

      // Show follow-up suggestion chips
      renderSuggestions(resp.suggestions || []);
      
      // Auto-speak short summary
      ttsPlayer.play(resp.short_summary || resp.text, currentLanguage);
    } else {
      chatMessages.push({
        role: 'bot',
        content: i18n.t('error')
      });
    }
  } catch (err) {
    chatIsLoading = false;
    chatMessages.push({ role: 'bot', content: 'Connection error' });
  }

  saveChatMessages();
  updateChatWindow();
}

function getFirstParagraph(text) {
  if (!text) return '';
  let clean = text.replace(/^#+\s+/gm, '').trim();
  let parts = clean.split(/\n\s*\n|\n(?=[#-]\s|\d+\.)/);
  for (let p of parts) {
    let line = p.trim().replace(/^[\*\-\#\>\d\.\s]+/, '').trim();
    if (line.length > 15) return line;
  }
  return text.substring(0, 180);
}

async function fetchMoreDetails(index) {
  ttsPlayer.stop();
  chatIsLoading = true;
  updateChatWindow();

  try {
    const data = await api.post('/chat/details', {
      session_id: 'default',
      language: currentLanguage
    });

    chatIsLoading = false;

    if (data && data.status === 'success') {
      const resp = data.response;
      
      // Update summary bubble to detailed content
      chatMessages[index].content = resp.text;
      chatMessages[index].response_type = 'detailed';
      
      // Read ONLY 1st paragraph aloud for detailed response as requested
      const firstParah = getFirstParagraph(resp.text);
      ttsPlayer.play(firstParah, currentLanguage);
    }
  } catch (err) {
    chatIsLoading = false;
  }

  saveChatMessages();
  updateChatWindow();
}

function updateChatWindow() {
  const stream = document.getElementById('chatMessages');
  const indicator = document.getElementById('chatTypingIndicator');
  if (!stream) return;

  stream.innerHTML = chatMessages.length === 0 ? renderChatWelcome() : renderChatList();
  if (indicator) {
    indicator.style.display = chatIsLoading ? 'flex' : 'none';
  }
  stream.scrollTop = stream.scrollHeight;
  i18n.translateDOM();
}

function speakMessage(index) {
  const msg = chatMessages[index];
  if (!msg) return;
  
  if (msg.response_type === 'detailed') {
    const firstParah = getFirstParagraph(msg.content);
    ttsPlayer.play(firstParah, currentLanguage);
  } else {
    ttsPlayer.play(msg.content, currentLanguage);
  }
}

function renderSuggestions(list) {
  const box = document.getElementById('chatSuggestionsContainer');
  if (!box) return;

  if (!list || list.length === 0) {
    box.innerHTML = '';
    return;
  }

  box.innerHTML = list.map(s => `
    <button class="btn btn-secondary btn-sm" onclick="useSuggestion('${s.replace(/'/g, "\\'")}')" style="font-size:0.75rem; border-radius:var(--radius-full);">
      💡 ${s}
    </button>
  `).join('');
}

function handleMicClick() {
  const btn = document.getElementById('chatMicBtn');
  if (sttRecorder.isRecording) {
    sttRecorder.stop();
  } else {
    sttRecorder.start(btn, (text) => {
      if (text && text.trim()) {
        const input = document.getElementById('chatInput');
        if (input) {
          input.value = text;
          sendChatMessage();
        }
      }
    });
  }
}

async function clearChatHistory() {
  ttsPlayer.stop();
  chatMessages = [];
  saveChatMessages();
  await api.delete('/chat/history?session_id=default');
  updateChatWindow();
  showToast('Chat history cleared', 'success');
}
