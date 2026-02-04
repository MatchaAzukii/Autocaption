// Autocaption Web UI
// Check if running in Tauri (use var to avoid shadowing global)
var inTauri = (typeof window.__TAURI__ !== 'undefined');

class AutocaptionUI {
    constructor() {
        this.ws = null;
        this.reconnectInterval = 3000;
        this.reconnectTimer = null;
        this.captions = [];
        this.maxHistory = 100;
        this.isOverlayMode = false;
        
        // Detect overlay mode from URL
        const urlParams = new URLSearchParams(window.location.search);
        this.isOverlayMode = urlParams.get('mode') === 'overlay';
        
        this.elements = {
            wsUrl: document.getElementById('ws-url'),
            connectBtn: document.getElementById('connect-btn'),
            connectionStatus: document.getElementById('connection-status'),
            liveCaption: document.getElementById('live-caption'),
            currentCaption: document.getElementById('current-caption'),
            historyList: document.getElementById('history-list'),
            overlayMode: document.getElementById('overlay-mode'),
            dashboardMode: document.getElementById('dashboard-mode'),
            enterOverlay: document.getElementById('enter-overlay'),
            exitOverlay: document.getElementById('exit-overlay'),
            toggleSettings: document.getElementById('toggle-settings'),
            fontSize: document.getElementById('font-size'),
            fontSizeValue: document.getElementById('font-size-value'),
            captionPosition: document.getElementById('caption-position'),
            textColor: document.getElementById('text-color'),
            bgColor: document.getElementById('bg-color'),
            bgOpacity: document.getElementById('bg-opacity'),
            captionsContainer: document.getElementById('captions-container'),
        };
        
        this.init();
    }
    
    init() {
        // If in overlay mode, switch UI immediately
        if (this.isOverlayMode) {
            this.enterOverlayMode();
            this.setupOverlayBehavior();
        }
        
        this.bindEvents();
        this.loadSettings();
        
        // If in Tauri, get backend URL from Rust
        if (inTauri) {
            this.loadTauriConfig();
        } else {
            this.connect();
        }
    }
    
    async loadTauriConfig() {
        console.log('Loading Tauri config...');
        console.log('inTauri:', inTauri);
        
        // Get WebSocket URL from Tauri backend
        if (inTauri) {
            try {
                const { invoke } = window.__TAURI__;
                const url = await invoke('get_backend_url');
                console.log('Got URL from Tauri:', url);
                this.elements.wsUrl.value = url;
            } catch (e) {
                console.log('Tauri get_backend_url failed:', e);
            }
        }
        
        // Delay connection slightly to ensure everything is ready
        setTimeout(() => {
            console.log('Auto-connecting to:', this.elements.wsUrl.value);
            this.connect();
        }, 500);
    }
    
    setupOverlayBehavior() {
        // In Tauri overlay mode, setup mouse interaction
        if (!inTauri) return;
        
        const container = this.elements.captionsContainer;
        
        // When mouse enters caption area, enable click-through
        container.addEventListener('mouseenter', () => {
            this.setClickThrough(false);
        });
        
        // When mouse leaves, disable click-through (make it pass-through)
        container.addEventListener('mouseleave', () => {
            // Small delay to allow clicking controls
            setTimeout(() => this.setClickThrough(true), 100);
        });
        
        // Initially enable click-through
        this.setClickThrough(true);
    }
    
    async setClickThrough(enabled) {
        if (!inTauri) return;
        try {
            const { invoke } = window.__TAURI__;
            await invoke('set_click_through', { enabled });
        } catch (e) {
            console.error('Failed to set click-through:', e);
        }
    }
    
    bindEvents() {
        // Connection
        this.elements.connectBtn.addEventListener('click', () => this.connect());
        
        // Overlay mode
        this.elements.enterOverlay.addEventListener('click', () => this.enterOverlayMode());
        this.elements.exitOverlay.addEventListener('click', () => this.exitOverlayMode());
        
        // Settings
        this.elements.fontSize.addEventListener('input', (e) => {
            this.elements.fontSizeValue.textContent = e.target.value + 'px';
            this.updateCaptionStyles();
        });
        
        this.elements.captionPosition.addEventListener('change', () => this.updateCaptionStyles());
        this.elements.textColor.addEventListener('input', () => this.updateCaptionStyles());
        this.elements.bgColor.addEventListener('input', () => this.updateCaptionStyles());
        this.elements.bgOpacity.addEventListener('input', () => this.updateCaptionStyles());
        
        // Toggle settings in overlay mode
        this.elements.toggleSettings.addEventListener('click', () => {
            this.exitOverlayMode();
        });
        
        // Keyboard shortcuts
        document.addEventListener('keydown', (e) => {
            if (e.key === 'Escape' && !this.elements.overlayMode.classList.contains('hidden')) {
                this.exitOverlayMode();
            }
        });
    }
    
    loadSettings() {
        const settings = JSON.parse(localStorage.getItem('autocaptionSettings') || '{}');
        
        if (settings.wsUrl) this.elements.wsUrl.value = settings.wsUrl;
        if (settings.fontSize) {
            this.elements.fontSize.value = settings.fontSize;
            this.elements.fontSizeValue.textContent = settings.fontSize + 'px';
        }
        if (settings.position) this.elements.captionPosition.value = settings.position;
        if (settings.textColor) this.elements.textColor.value = settings.textColor;
        if (settings.bgColor) this.elements.bgColor.value = settings.bgColor;
        if (settings.bgOpacity) this.elements.bgOpacity.value = settings.bgOpacity;
        
        this.updateCaptionStyles();
    }
    
    saveSettings() {
        const settings = {
            wsUrl: this.elements.wsUrl.value,
            fontSize: this.elements.fontSize.value,
            position: this.elements.captionPosition.value,
            textColor: this.elements.textColor.value,
            bgColor: this.elements.bgColor.value,
            bgOpacity: this.elements.bgOpacity.value,
        };
        localStorage.setItem('autocaptionSettings', JSON.stringify(settings));
    }
    
    updateCaptionStyles() {
        const fontSize = this.elements.fontSize.value + 'px';
        const position = this.elements.captionPosition.value;
        const textColor = this.elements.textColor.value;
        const bgColor = this.elements.bgColor.value;
        const bgOpacity = this.elements.bgOpacity.value / 100;
        
        // Convert hex to rgba
        const r = parseInt(bgColor.slice(1, 3), 16);
        const g = parseInt(bgColor.slice(3, 5), 16);
        const b = parseInt(bgColor.slice(5, 7), 16);
        const bgRgba = `rgba(${r}, ${g}, ${b}, ${bgOpacity})`;
        
        // Apply styles
        document.documentElement.style.setProperty('--caption-font-size', fontSize);
        document.documentElement.style.setProperty('--caption-text-color', textColor);
        
        this.elements.captionsContainer.className = 'position-' + position;
        this.elements.captionsContainer.style.background = bgRgba;
        
        this.saveSettings();
    }
    
    connect() {
        if (this.ws) {
            console.log('Closing existing WebSocket');
            this.ws.close();
        }
        
        const url = this.elements.wsUrl.value;
        console.log('Connecting to WebSocket:', url);
        
        this.elements.connectionStatus.textContent = 'Connecting...';
        this.elements.connectionStatus.className = '';
        
        try {
            this.ws = new WebSocket(url);
            
            this.ws.onopen = () => {
                console.log('WebSocket connected');
                this.elements.connectionStatus.textContent = 'Connected';
                this.elements.connectionStatus.className = 'connected';
                this.clearReconnectTimer();
            };
            
            this.ws.onmessage = (event) => {
                try {
                    const data = JSON.parse(event.data);
                    this.handleCaption(data);
                } catch (e) {
                    console.error('Failed to parse message:', e);
                }
            };
            
            this.ws.onerror = (error) => {
                console.error('WebSocket error:', error);
                this.elements.connectionStatus.textContent = 'Error';
                this.elements.connectionStatus.className = 'disconnected';
            };
            
            this.ws.onclose = () => {
                console.log('WebSocket closed');
                this.elements.connectionStatus.textContent = 'Disconnected';
                this.elements.connectionStatus.className = 'disconnected';
                this.scheduleReconnect();
            };
        } catch (e) {
            console.error('Failed to connect:', e);
            this.elements.connectionStatus.textContent = 'Failed';
            this.elements.connectionStatus.className = 'disconnected';
            this.scheduleReconnect();
        }
    }
    
    scheduleReconnect() {
        if (this.reconnectTimer) return;
        
        this.reconnectTimer = setTimeout(() => {
            this.reconnectTimer = null;
            this.connect();
        }, this.reconnectInterval);
    }
    
    clearReconnectTimer() {
        if (this.reconnectTimer) {
            clearTimeout(this.reconnectTimer);
            this.reconnectTimer = null;
        }
    }
    
    handleCaption(data) {
        // Update live caption
        const text = data.source_text || data.text || '';
        
        if (text) {
            this.elements.liveCaption.textContent = text;
            this.elements.liveCaption.classList.remove('empty');
            this.elements.currentCaption.textContent = text;
        }
        
        // Add to history
        const caption = {
            id: data.id,
            start: data.start,
            end: data.end,
            text: text,
            language: data.language || 'auto',
            timestamp: new Date()
        };
        
        this.captions.unshift(caption);
        if (this.captions.length > this.maxHistory) {
            this.captions.pop();
        }
        
        this.renderHistory();
    }
    
    renderHistory() {
        this.elements.historyList.innerHTML = '';
        
        this.captions.forEach(caption => {
            const item = document.createElement('div');
            item.className = 'caption-item';
            
            const time = this.formatTime(caption.start);
            
            item.innerHTML = `
                <span class="caption-time">${time}</span>
                <span class="caption-text">${this.escapeHtml(caption.text)}</span>
                <span class="caption-lang">${caption.language}</span>
            `;
            
            this.elements.historyList.appendChild(item);
        });
    }
    
    formatTime(seconds) {
        const hrs = Math.floor(seconds / 3600);
        const mins = Math.floor((seconds % 3600) / 60);
        const secs = Math.floor(seconds % 60);
        const ms = Math.floor((seconds % 1) * 1000);
        
        return `${hrs.toString().padStart(2, '0')}:${mins.toString().padStart(2, '0')}:${secs.toString().padStart(2, '0')}.${ms.toString().padStart(3, '0')}`;
    }
    
    escapeHtml(text) {
        const div = document.createElement('div');
        div.textContent = text;
        return div.innerHTML;
    }
    
    async enterOverlayMode() {
        // If in Tauri, tell Rust to show overlay window
        if (inTauri) {
            try {
                const { invoke } = window.__TAURI__;
                await invoke('toggle_overlay_command');
                return;
            } catch (e) {
                console.log('Tauri toggle failed, using web mode');
            }
        }
        
        // Web mode: switch visibility in same window
        this.elements.dashboardMode.classList.add('hidden');
        this.elements.overlayMode.classList.remove('hidden');
        this.isOverlayMode = true;
    }
    
    async exitOverlayMode() {
        // If in Tauri, tell Rust to hide overlay window
        if (inTauri) {
            try {
                const { invoke } = window.__TAURI__;
                await invoke('toggle_overlay_command');
                return;
            } catch (e) {
                console.log('Tauri toggle failed, using web mode');
            }
        }
        
        // Web mode
        this.elements.overlayMode.classList.add('hidden');
        this.elements.dashboardMode.classList.remove('hidden');
        this.isOverlayMode = false;
    }
}

// Initialize when DOM is ready
document.addEventListener('DOMContentLoaded', () => {
    window.autocaption = new AutocaptionUI();
});
