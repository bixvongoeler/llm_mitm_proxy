"""
Widget bundle generator.

Creates the complete HTML/CSS/JS bundle for the chat widget.
Handles session management, transcript persistence, and welcome flow.
"""

from ..config import get_config


def get_widget_bundle() -> str:
    """
    Get the complete widget bundle as HTML string.

    Returns:
        HTML string containing styles, markup, and JavaScript.
    """
    config = get_config()
    chat_server_url = f"http://{config.chat_server_host}:{config.chat_server_port}"

    return f"""
<!-- SIS Advisor Chat Widget -->
<script>
{_get_javascript(chat_server_url)}
</script>
<!-- End SIS Advisor Chat Widget -->
"""


def _get_styles() -> str:
    """Get the widget CSS styles."""
    return """
/* SIS Advisor Chat Widget Styles */
#sis-advisor-root {
    position: fixed !important;
    bottom: 20px !important;
    right: 20px !important;
    z-index: 2147483647 !important;
    font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, 'Helvetica Neue', Arial, sans-serif !important;
    font-size: 14px !important;
    line-height: 1.5 !important;
    all: initial;
    font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, 'Helvetica Neue', Arial, sans-serif;
    font-size: 14px;
    line-height: 1.5;
    position: fixed;
    bottom: 20px;
    right: 20px;
    z-index: 2147483647;
}

#sis-advisor-root * {
    box-sizing: border-box !important;
}

/* Toggle Button */
#sis-advisor-root .sis-chat-toggle {
    width: 60px;
    height: 60px;
    border-radius: 50%;
    background: linear-gradient(135deg, #3b82f6 0%, #1d4ed8 100%);
    color: white;
    border: none;
    cursor: pointer;
    font-size: 28px;
    box-shadow: 0 4px 16px rgba(59, 130, 246, 0.4);
    transition: transform 0.2s, box-shadow 0.2s;
    display: flex;
    align-items: center;
    justify-content: center;
    padding: 0;
    margin: 0;
}

#sis-advisor-root .sis-chat-toggle:hover {
    transform: scale(1.05);
    box-shadow: 0 6px 20px rgba(59, 130, 246, 0.5);
}

#sis-advisor-root .sis-chat-toggle.open {
    background: linear-gradient(135deg, #ef4444 0%, #dc2626 100%);
    box-shadow: 0 4px 16px rgba(239, 68, 68, 0.4);
}

/* Chat Window */
#sis-advisor-root .sis-chat-window {
    display: none;
    position: absolute;
    bottom: 70px;
    right: 0;
    width: 475px;
    height: 560px;
    background: white;
    border-radius: 16px;
    box-shadow: 0 8px 32px rgba(0, 0, 0, 0.2);
    overflow: hidden;
    flex-direction: column;
}

#sis-advisor-root .sis-chat-window.open {
    display: flex;
}

/* Header */
#sis-advisor-root .sis-chat-header {
    background: linear-gradient(135deg, #3b82f6 0%, #1d4ed8 100%);
    color: white;
    padding: 16px;
    display: flex;
    align-items: center;
    justify-content: space-between;
    flex-shrink: 0;
}

#sis-advisor-root .sis-chat-title {
    font-weight: 600;
    font-size: 16px;
    display: flex;
    align-items: center;
    gap: 8px;
    color: white;
}

#sis-advisor-root .sis-chat-actions {
    display: flex;
    gap: 8px;
}

#sis-advisor-root .sis-chat-action-btn {
    background: rgba(255, 255, 255, 0.2);
    border: none;
    color: white;
    width: 32px;
    height: 32px;
    border-radius: 8px;
    cursor: pointer;
    font-size: 14px;
    transition: background 0.2s;
    padding: 0;
    margin: 0;
}

#sis-advisor-root .sis-chat-action-btn:hover {
    background: rgba(255, 255, 255, 0.3);
}

/* Messages */
#sis-advisor-root .sis-chat-messages {
    flex: 1;
    overflow-y: auto;
    padding: 16px;
    background: #f8fafc;
}

#sis-advisor-root .sis-chat-message {
    margin-bottom: 12px;
    max-width: 85%;
    animation: sis-msg-fade-in 0.3s ease-out;
}

@keyframes sis-msg-fade-in {
    from { opacity: 0; transform: translateY(10px); }
    to { opacity: 1; transform: translateY(0); }
}

#sis-advisor-root .sis-chat-message.assistant {
    margin-right: auto;
    margin-left: 0;
}

#sis-advisor-root .sis-chat-message.user {
    margin-left: auto;
    margin-right: 0;
}

#sis-advisor-root .sis-chat-bubble {
    padding: 12px 16px;
    border-radius: 16px;
    word-wrap: break-word;
    white-space: pre-wrap;
}

#sis-advisor-root .sis-chat-message.assistant .sis-chat-bubble {
    background: white;
    color: #1e293b;
    border-bottom-left-radius: 4px;
    box-shadow: 0 1px 2px rgba(0, 0, 0, 0.1);
}

#sis-advisor-root .sis-chat-message.user .sis-chat-bubble {
    background: linear-gradient(135deg, #3b82f6 0%, #1d4ed8 100%);
    color: white;
    border-bottom-right-radius: 4px;
}

/* Typing indicator */
#sis-advisor-root .sis-chat-typing {
    display: none;
    margin-bottom: 12px;
}

#sis-advisor-root .sis-chat-typing.active {
    display: block;
}

#sis-advisor-root .sis-typing-dots {
    background: white;
    padding: 12px 16px;
    border-radius: 16px;
    border-bottom-left-radius: 4px;
    box-shadow: 0 1px 2px rgba(0, 0, 0, 0.1);
    display: inline-flex;
    gap: 4px;
}

#sis-advisor-root .sis-typing-dot {
    width: 8px;
    height: 8px;
    background: #94a3b8;
    border-radius: 50%;
    animation: sis-typing-bounce 1.4s infinite ease-in-out;
}

#sis-advisor-root .sis-typing-dot:nth-child(1) { animation-delay: -0.32s; }
#sis-advisor-root .sis-typing-dot:nth-child(2) { animation-delay: -0.16s; }

@keyframes sis-typing-bounce {
    0%, 80%, 100% { transform: scale(0.8); opacity: 0.5; }
    40% { transform: scale(1); opacity: 1; }
}

/* Input area */
#sis-advisor-root .sis-chat-input-area {
    padding: 12px;
    border-top: 1px solid #e2e8f0;
    background: white;
    flex-shrink: 0;
}

#sis-advisor-root .sis-chat-input-row {
    display: flex;
    gap: 8px;
}

#sis-advisor-root .sis-chat-input {
    flex: 1;
    padding: 12px 16px;
    border: 1px solid #e2e8f0;
    border-radius: 24px;
    outline: none;
    font-size: 14px;
    font-family: inherit;
    resize: none;
    max-height: 100px;
    transition: border-color 0.2s;
    background: white;
    color: #1e293b;
}

#sis-advisor-root .sis-chat-input:focus {
    border-color: #3b82f6;
}

#sis-advisor-root .sis-chat-input::placeholder {
    color: #94a3b8;
}

#sis-advisor-root .sis-chat-input:disabled {
    background: #f1f5f9;
    cursor: not-allowed;
}

#sis-advisor-root .sis-chat-send {
    width: 44px;
    height: 44px;
    border: none;
    background: linear-gradient(135deg, #3b82f6 0%, #1d4ed8 100%);
    color: white;
    border-radius: 50%;
    cursor: pointer;
    font-size: 18px;
    transition: transform 0.2s;
    display: flex;
    align-items: center;
    justify-content: center;
    padding: 0;
    margin: 0;
    flex-shrink: 0;
}

#sis-advisor-root .sis-chat-send:hover:not(:disabled) {
    transform: scale(1.05);
}

#sis-advisor-root .sis-chat-send:disabled {
    opacity: 0.5;
    cursor: not-allowed;
}

/* Upload button */
#sis-advisor-root .sis-upload-row {
    margin-top: 8px;
    display: flex;
    justify-content: center;
}

#sis-advisor-root .sis-upload-btn {
    background: none;
    border: 2px dashed #3b82f6;
    color: #3b82f6;
    padding: 10px 20px;
    border-radius: 8px;
    font-size: 13px;
    font-weight: 500;
    cursor: pointer;
    transition: all 0.2s;
    display: flex;
    align-items: center;
    gap: 8px;
}

#sis-advisor-root .sis-upload-btn:hover {
    background: #eff6ff;
}

#sis-advisor-root .sis-upload-btn.uploaded {
    border-color: #22c55e;
    color: #22c55e;
}

/* Suggested questions */
#sis-advisor-root .sis-suggestions {
    display: flex;
    flex-direction: column;
    gap: 8px;
    margin: 12px 0;
}

#sis-advisor-root .sis-suggestion-btn {
    background: #f1f5f9;
    border: 1px solid #e2e8f0;
    border-radius: 12px;
    padding: 10px 16px;
    font-size: 13px;
    color: #475569;
    cursor: pointer;
    text-align: left;
    transition: all 0.2s;
    font-family: inherit;
}

#sis-advisor-root .sis-suggestion-btn:hover {
    background: #e2e8f0;
    border-color: #3b82f6;
    color: #1d4ed8;
}

/* Hidden file input */
#sis-advisor-root .sis-file-input {
    display: none;
}

/* Upload row - hidden after upload */
#sis-advisor-root .sis-upload-row.hidden {
    display: none;
}

/* Markdown styles in chat bubbles */
#sis-advisor-root .sis-chat-bubble strong {
    font-weight: 600;
}

#sis-advisor-root .sis-chat-bubble em {
    font-style: italic;
}

#sis-advisor-root .sis-chat-bubble ul,
#sis-advisor-root .sis-chat-bubble ol {
    margin: 8px 0;
    padding-left: 20px;
}

#sis-advisor-root .sis-chat-bubble li {
    margin: 4px 0;
}

#sis-advisor-root .sis-chat-bubble p {
    margin: 8px 0;
}

#sis-advisor-root .sis-chat-bubble p:first-child {
    margin-top: 0;
}

#sis-advisor-root .sis-chat-bubble p:last-child {
    margin-bottom: 0;
}

#sis-advisor-root .sis-chat-bubble code {
    background: rgba(0, 0, 0, 0.05);
    padding: 2px 6px;
    border-radius: 4px;
    font-family: 'Monaco', 'Menlo', monospace;
    font-size: 0.9em;
}
"""


def _get_javascript(chat_server_url: str) -> str:
    """Get the widget JavaScript."""
    styles = _get_styles().replace("`", "\\`").replace("${", "\\${")

    return f"""
(function() {{
    'use strict';

    // Prevent double initialization
    if (window.__sisAdvisorInitialized) return;
    window.__sisAdvisorInitialized = true;

    // Configuration
    const CHAT_API = '{chat_server_url}';
    const STORAGE_KEY = 'sis_advisor_chat';
    const SESSION_KEY = 'sis_advisor_session';
    const TRANSCRIPT_KEY = 'sis_advisor_transcript';

    // State
    let sessionId = null;
    let transcriptSummary = null;
    let messages = [];
    let isOpen = false;
    let isSending = false;
    let hasTranscript = false;
    let sessionInitialized = false;

    // DOM elements
    let root, toggle, chatWindow, messagesContainer, input, sendBtn, typingIndicator, uploadBtn, uploadRow;

    // Create and inject styles
    function injectStyles() {{
        const style = document.createElement('style');
        style.id = 'sis-advisor-styles';
        style.textContent = `{styles}`;
        document.head.appendChild(style);
    }}

    // Create root element
    function createRoot() {{
        const existing = document.getElementById('sis-advisor-root');
        if (existing) existing.remove();

        root = document.createElement('div');
        root.id = 'sis-advisor-root';
        document.body.appendChild(root);
        return root;
    }}

    // Initialize
    async function init() {{
        if (!document.body) {{
            setTimeout(init, 50);
            return;
        }}

        if (!document.getElementById('sis-advisor-styles')) {{
            injectStyles();
        }}

        createRoot();

        // Load saved state
        loadState();

        // Render UI
        render();
        setupEvents();

        // Initialize session if needed
        if (!sessionInitialized) {{
            await initializeSession();
        }}

        // Load welcome message if no messages
        if (messages.length === 0) {{
            await loadWelcome();
        }}

        updateChatState();
        console.log('[SIS Advisor] Initialized, session:', sessionId, 'hasTranscript:', hasTranscript);
    }}

    // Initialize session by uploading course content
    async function initializeSession() {{
        if (!sessionId) {{
            sessionId = 'sis_' + Math.random().toString(36).substr(2, 9) + '_' + Date.now();
        }}

        try {{
            const response = await fetch(CHAT_API + '/init', {{
                method: 'POST',
                headers: {{ 'Content-Type': 'application/json' }},
                body: JSON.stringify({{ session_id: sessionId }})
            }});

            const data = await response.json();
            if (data.success) {{
                sessionInitialized = true;
                saveState();
                console.log('[SIS Advisor] Session initialized');
            }} else {{
                console.error('[SIS Advisor] Session init failed:', data.error);
            }}
        }} catch (err) {{
            console.error('[SIS Advisor] Session init error:', err);
        }}
    }}

    // Load welcome message from server
    async function loadWelcome() {{
        try {{
            const response = await fetch(CHAT_API + '/welcome');
            const data = await response.json();

            if (data.response) {{
                addMessage('assistant', data.response, false);
            }}
        }} catch (err) {{
            console.error('[SIS Advisor] Welcome fetch error:', err);
            addMessage('assistant', "Welcome! Please upload your transcript to get started.", false);
        }}
    }}

    function render() {{
        root.innerHTML = `
            <button class="sis-chat-toggle" id="sis-toggle" title="SIS Academic Advisor">
                <span>&#x1F393;</span>
            </button>
            <div class="sis-chat-window" id="sis-window">
                <div class="sis-chat-header">
                    <div class="sis-chat-title">
                        <span>&#x1F393;</span>
                        <span>SIS Academic Advisor</span>
                    </div>
                    <div class="sis-chat-actions">
                        <button class="sis-chat-action-btn" id="sis-clear" title="Clear chat">&#x1F5D1;</button>
                    </div>
                </div>
                <div class="sis-chat-messages" id="sis-messages">
                    <div class="sis-chat-typing" id="sis-typing">
                        <div class="sis-typing-dots">
                            <div class="sis-typing-dot"></div>
                            <div class="sis-typing-dot"></div>
                            <div class="sis-typing-dot"></div>
                        </div>
                    </div>
                </div>
                <div class="sis-chat-input-area">
                    <div class="sis-chat-input-row">
                        <textarea class="sis-chat-input" id="sis-input" placeholder="Upload transcript first..." rows="1" disabled></textarea>
                        <button class="sis-chat-send" id="sis-send" title="Send" disabled>&#x27A4;</button>
                    </div>
                    <div class="sis-upload-row">
                        <button class="sis-upload-btn" id="sis-upload-btn">
                            <span>&#x1F4C4;</span> Upload transcript (PDF)
                        </button>
                        <input type="file" class="sis-file-input" id="sis-file-input" accept=".pdf">
                    </div>
                </div>
            </div>
        `;

        toggle = document.getElementById('sis-toggle');
        chatWindow = document.getElementById('sis-window');
        messagesContainer = document.getElementById('sis-messages');
        input = document.getElementById('sis-input');
        sendBtn = document.getElementById('sis-send');
        typingIndicator = document.getElementById('sis-typing');
        uploadBtn = document.getElementById('sis-upload-btn');
        uploadRow = root.querySelector('.sis-upload-row');

        renderMessages();
    }}

    function setupEvents() {{
        toggle.addEventListener('click', toggleChat);
        sendBtn.addEventListener('click', sendMessage);

        input.addEventListener('keydown', function(e) {{
            if (e.key === 'Enter' && !e.shiftKey) {{
                e.preventDefault();
                sendMessage();
            }}
        }});

        input.addEventListener('input', function() {{
            this.style.height = 'auto';
            this.style.height = Math.min(this.scrollHeight, 100) + 'px';
        }});

        document.getElementById('sis-clear').addEventListener('click', clearChat);

        document.getElementById('sis-upload-btn').addEventListener('click', function() {{
            document.getElementById('sis-file-input').click();
        }});

        document.getElementById('sis-file-input').addEventListener('change', handleFileUpload);
    }}

    function updateChatState() {{
        if (hasTranscript) {{
            input.disabled = false;
            sendBtn.disabled = false;
            input.placeholder = 'Ask about courses, requirements...';
            // Hide the upload row completely after transcript uploaded
            uploadRow.classList.add('hidden');
        }} else {{
            input.disabled = true;
            sendBtn.disabled = true;
            input.placeholder = 'Upload transcript first...';
            uploadRow.classList.remove('hidden');
            uploadBtn.innerHTML = '<span>&#x1F4C4;</span> Upload transcript (PDF)';
        }}
    }}

    function toggleChat() {{
        isOpen = !isOpen;
        chatWindow.classList.toggle('open', isOpen);
        toggle.classList.toggle('open', isOpen);
        toggle.innerHTML = isOpen ? '<span>&#x2715;</span>' : '<span>&#x1F393;</span>';

        if (isOpen && hasTranscript) {{
            input.focus();
        }}
        scrollToBottom();
    }}

    function addMessage(role, content, save = true) {{
        messages.push({{ role, content, timestamp: Date.now() }});
        if (save) saveState();
        renderMessages();
        scrollToBottom();
    }}

    // Simple markdown parser for chat messages
    function parseMarkdown(text) {{
        // Escape HTML first to prevent XSS
        let html = text
            .replace(/&/g, '&amp;')
            .replace(/</g, '&lt;')
            .replace(/>/g, '&gt;');

        // Bold: **text** or __text__
        html = html.replace(/\*\*(.+?)\*\*/g, '<strong>$1</strong>');
        html = html.replace(/__(.+?)__/g, '<strong>$1</strong>');

        // Italic: *text* or _text_ (but not inside words)
        html = html.replace(/(?<![\\w*])\*([^*]+)\*(?![\\w*])/g, '<em>$1</em>');
        html = html.replace(/(?<![\\w_])_([^_]+)_(?![\\w_])/g, '<em>$1</em>');

        // Inline code: `code`
        html = html.replace(/`([^`]+)`/g, '<code>$1</code>');

        // Convert bullet lists: lines starting with - or *
        const lines = html.split('\\n');
        let inList = false;
        let result = [];

        for (let i = 0; i < lines.length; i++) {{
            const line = lines[i];
            const listMatch = line.match(/^\\s*[-*]\\s+(.+)$/);

            if (listMatch) {{
                if (!inList) {{
                    result.push('<ul>');
                    inList = true;
                }}
                result.push('<li>' + listMatch[1] + '</li>');
            }} else {{
                if (inList) {{
                    result.push('</ul>');
                    inList = false;
                }}
                // Convert double newlines to paragraphs
                if (line.trim() === '' && i > 0 && result.length > 0) {{
                    result.push('<br>');
                }} else if (line.trim()) {{
                    result.push(line);
                }}
            }}
        }}

        if (inList) {{
            result.push('</ul>');
        }}

        return result.join('\\n');
    }}

    function renderMessages() {{
        const children = Array.from(messagesContainer.children);
        children.forEach(child => {{
            if (child.id !== 'sis-typing') child.remove();
        }});

        messages.forEach(msg => {{
            const div = document.createElement('div');
            div.className = 'sis-chat-message ' + msg.role;
            const bubble = document.createElement('div');
            bubble.className = 'sis-chat-bubble';
            // Use parsed markdown for assistant messages, plain text for user
            if (msg.role === 'assistant') {{
                bubble.innerHTML = parseMarkdown(msg.content);
            }} else {{
                bubble.textContent = msg.content;
            }}
            div.appendChild(bubble);
            messagesContainer.insertBefore(div, typingIndicator);
        }});
    }}

    function showSuggestedQuestions() {{
        const suggestions = [
            "Help me pick courses for this semester",
            "Summarize my current academic history",
            "What degree requirements do I still have"
        ];

        const container = document.createElement('div');
        container.className = 'sis-suggestions';

        suggestions.forEach(q => {{
            const btn = document.createElement('button');
            btn.className = 'sis-suggestion-btn';
            btn.textContent = q;
            btn.onclick = () => {{
                input.value = q;
                container.remove();
                sendMessage();
            }};
            container.appendChild(btn);
        }});

        messagesContainer.insertBefore(container, typingIndicator);
        scrollToBottom();
    }}

    function scrollToBottom() {{
        messagesContainer.scrollTop = messagesContainer.scrollHeight;
    }}

    function showTyping(show) {{
        typingIndicator.classList.toggle('active', show);
        if (show) scrollToBottom();
    }}

    async function sendMessage() {{
        const text = input.value.trim();
        if (!text || isSending || !hasTranscript) return;

        isSending = true;
        sendBtn.disabled = true;

        addMessage('user', text);
        input.value = '';
        input.style.height = 'auto';

        showTyping(true);

        try {{
            const pageContent = extractPageContent();

            const response = await fetch(CHAT_API + '/chat', {{
                method: 'POST',
                headers: {{ 'Content-Type': 'application/json' }},
                body: JSON.stringify({{
                    message: text,
                    session_id: sessionId,
                    page_content: pageContent,
                    transcript_summary: transcriptSummary
                }})
            }});

            const data = await response.json();
            showTyping(false);

            if (data.error) {{
                addMessage('assistant', 'Sorry, I encountered an error: ' + data.error);
            }} else {{
                addMessage('assistant', data.response);
            }}
        }} catch (err) {{
            showTyping(false);
            addMessage('assistant', 'Sorry, I could not connect to the server. Please try again.');
            console.error('[SIS Advisor] Error:', err);
        }}

        isSending = false;
        sendBtn.disabled = false;
        input.focus();
    }}

    async function handleFileUpload(e) {{
        const file = e.target.files[0];
        if (!file) return;

        if (!file.name.toLowerCase().endsWith('.pdf')) {{
            addMessage('assistant', 'Please upload a PDF file.');
            return;
        }}

        if (file.size > 4 * 1024 * 1024) {{
            addMessage('assistant', 'File is too large. Maximum size is 4MB.');
            return;
        }}

        // Show upload message
        addMessage('user', '(Uploading: ' + file.name + ')');

        // Show processing message - docling conversion can take a while
        addMessage('assistant', '**Processing your transcript...**\\n\\nThis may take 30-60 seconds as I extract and analyze your academic records. Please wait...');

        // Disable upload button during processing
        uploadBtn.disabled = true;
        uploadBtn.innerHTML = '<span>&#x23F3;</span> Processing...';

        showTyping(true);

        try {{
            const formData = new FormData();
            formData.append('file', file);
            formData.append('session_id', sessionId);

            const response = await fetch(CHAT_API + '/upload', {{
                method: 'POST',
                body: formData
            }});

            const data = await response.json();
            showTyping(false);

            // Remove the processing message
            if (messages.length > 0 && messages[messages.length - 1].content.includes('Processing your transcript')) {{
                messages.pop();
            }}

            if (data.error) {{
                uploadBtn.disabled = false;
                updateChatState();
                addMessage('assistant', 'Upload failed: ' + data.error);
            }} else {{
                // Store transcript summary
                transcriptSummary = data.summary;
                hasTranscript = true;
                saveState();
                updateChatState();

                addMessage('assistant', data.message + "\\n\\nWhat would you like to know?");

                // Show suggested questions
                showSuggestedQuestions();
            }}
        }} catch (err) {{
            showTyping(false);

            // Remove the processing message
            if (messages.length > 0 && messages[messages.length - 1].content.includes('Processing your transcript')) {{
                messages.pop();
            }}

            uploadBtn.disabled = false;
            updateChatState();
            addMessage('assistant', 'Upload failed. Please try again.');
            console.error('[SIS Advisor] Upload error:', err);
        }}

        e.target.value = '';
    }}

    function extractPageContent() {{
        // Send raw HTML for server-side parsing
        // SIS-specific selectors first (IntraSee course search accordion)
        const selectors = [
            '#TFP_CLSSRCH_accordion',           // SIS course search results container
            '#tfp_searchresultsHeader_region',  // Header with result count
            '.tfp_accordion_row',               // Individual course rows
            '.ps_box-scrollarea-row',           // Generic PeopleSoft (fallback)
            '#ptPageRow',
            '#pt_pageinfo_win0',
            '.ps_grid-body',
            'table.ps_grid-flex',
            '.psc_rowact',
            '[id*="DESCR"]',
            '[id*="SSR_CLSRCH"]',
            '.PABACKGROUNDINVISIBLE'
        ];

        let html = '';
        const seen = new Set();

        for (const selector of selectors) {{
            try {{
                const elements = document.querySelectorAll(selector);
                elements.forEach(el => {{
                    if (seen.has(el)) return;
                    seen.add(el);
                    html += el.outerHTML + '\\n';
                }});
            }} catch (e) {{}}
        }}

        if (html.length < 100) {{
            html = document.body.innerHTML;
        }}

        // Truncate to reasonable size (increased for full course search)
        return html.substring(0, 200000);
    }}

    async function clearChat() {{
        if (!confirm('Clear chat history and transcript? You will need to re-upload your transcript.')) return;

        // Generate new session
        sessionId = 'sis_' + Math.random().toString(36).substr(2, 9) + '_' + Date.now();
        sessionInitialized = false;

        // Clear transcript
        transcriptSummary = null;
        hasTranscript = false;

        // Clear messages
        messages = [];

        // Clear storage
        localStorage.removeItem(STORAGE_KEY);
        localStorage.removeItem(SESSION_KEY);
        localStorage.removeItem(TRANSCRIPT_KEY);

        // Re-initialize
        await initializeSession();
        await loadWelcome();

        renderMessages();
        updateChatState();
        saveState();
    }}

    function saveState() {{
        try {{
            localStorage.setItem(STORAGE_KEY, JSON.stringify({{
                messages: messages.slice(-50),
                savedAt: Date.now()
            }}));
            localStorage.setItem(SESSION_KEY, JSON.stringify({{
                sessionId: sessionId,
                initialized: sessionInitialized
            }}));
            if (transcriptSummary) {{
                localStorage.setItem(TRANSCRIPT_KEY, transcriptSummary);
            }}
        }} catch (e) {{
            console.warn('[SIS Advisor] Could not save state:', e);
        }}
    }}

    function loadState() {{
        try {{
            // Load session
            const sessionData = localStorage.getItem(SESSION_KEY);
            if (sessionData) {{
                const data = JSON.parse(sessionData);
                sessionId = data.sessionId;
                sessionInitialized = data.initialized || false;
            }}

            // Load messages
            const chatData = localStorage.getItem(STORAGE_KEY);
            if (chatData) {{
                const data = JSON.parse(chatData);
                const isRecent = Date.now() - data.savedAt < 24 * 60 * 60 * 1000;
                if (isRecent) {{
                    messages = data.messages || [];
                }}
            }}

            // Load transcript
            const transcript = localStorage.getItem(TRANSCRIPT_KEY);
            if (transcript) {{
                transcriptSummary = transcript;
                hasTranscript = true;
            }}
        }} catch (e) {{
            console.warn('[SIS Advisor] Could not load state:', e);
        }}
    }}

    // Start initialization
    if (document.readyState === 'loading') {{
        document.addEventListener('DOMContentLoaded', init);
    }} else {{
        setTimeout(init, 100);
    }}
}})();
"""
