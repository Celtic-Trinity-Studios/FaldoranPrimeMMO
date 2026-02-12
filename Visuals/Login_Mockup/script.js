document.addEventListener('DOMContentLoaded', () => {
    const particlesContainer = document.getElementById('particles');
    const particleCount = 80;
    const loginCard = document.getElementById('loginCard');
    const logo = document.querySelector('.game-logo');
    const resultMessage = document.getElementById('resultMessage');

    // ============================================================
    // Parallax Effect — subtle depth on mouse movement
    // ============================================================
    let targetMoveX = 0;
    let targetMoveY = 0;
    let currentMoveX = 0;
    let currentMoveY = 0;

    document.addEventListener('mousemove', (e) => {
        targetMoveX = (e.clientX - window.innerWidth / 2) * 0.008;
        targetMoveY = (e.clientY - window.innerHeight / 2) * 0.008;
    });

    // Smooth interpolation for parallax
    function animateParallax() {
        currentMoveX += (targetMoveX - currentMoveX) * 0.08;
        currentMoveY += (targetMoveY - currentMoveY) * 0.08;

        if (loginCard) {
            loginCard.style.transform = `translate(${currentMoveX}px, ${currentMoveY}px)`;
        }
        if (logo) {
            logo.style.transform = `translate(${currentMoveX * -1.2}px, ${currentMoveY * -1.2}px)`;
        }

        const background = document.querySelector('.background-container');
        if (background) {
            background.style.transform = `scale(1.05) translate(${currentMoveX * -0.3}px, ${currentMoveY * -0.3}px)`;
        }

        requestAnimationFrame(animateParallax);
    }
    requestAnimationFrame(animateParallax);

    // ============================================================
    // Background Particles — floating mana motes
    // ============================================================
    for (let i = 0; i < particleCount; i++) {
        const particle = document.createElement('div');
        particle.className = 'particle';

        const posX = Math.random() * 100;
        const posY = Math.random() * 100;
        const delay = Math.random() * 15;
        const duration = 20 + Math.random() * 30;
        const size = 1 + Math.random() * 3;
        const opacity = 0.03 + Math.random() * 0.15;

        // Mix of gold and blue particles
        const isGold = Math.random() > 0.4;
        const color = isGold
            ? `rgba(197, 160, 89, ${opacity})`
            : `rgba(74, 144, 226, ${opacity * 0.6})`;

        particle.style.cssText = `
            position: absolute;
            left: ${posX}%;
            top: ${posY}%;
            width: ${size}px;
            height: ${size}px;
            background: ${color};
            border-radius: 50%;
            filter: blur(${size < 2 ? 0.5 : 1}px);
            animation: floatParticle ${duration}s infinite linear;
            animation-delay: -${delay}s;
            pointer-events: none;
        `;

        particlesContainer.appendChild(particle);
    }

    // ============================================================
    // Rune Ring — SVG-based rotating geometric ring
    // ============================================================
    const runeRing = document.getElementById('runeRing');
    if (runeRing) {
        const svg = document.createElementNS('http://www.w3.org/2000/svg', 'svg');
        svg.setAttribute('viewBox', '0 0 700 700');
        svg.setAttribute('width', '700');
        svg.setAttribute('height', '700');
        svg.style.cssText = 'width: 100%; height: 100%;';

        const cx = 350, cy = 350;
        const radii = [300, 280, 250];

        radii.forEach((r, idx) => {
            const circle = document.createElementNS('http://www.w3.org/2000/svg', 'circle');
            circle.setAttribute('cx', cx);
            circle.setAttribute('cy', cy);
            circle.setAttribute('r', r);
            circle.setAttribute('fill', 'none');
            circle.setAttribute('stroke', 'rgba(197, 160, 89, 0.3)');
            circle.setAttribute('stroke-width', idx === 0 ? '0.5' : '0.3');

            if (idx > 0) {
                const dashLen = (2 * Math.PI * r) / (12 + idx * 6);
                circle.setAttribute('stroke-dasharray', `${dashLen * 0.6} ${dashLen * 0.4}`);
            }

            svg.appendChild(circle);
        });

        // Add tick marks around the outer ring
        const tickCount = 36;
        for (let i = 0; i < tickCount; i++) {
            const angle = (i / tickCount) * Math.PI * 2;
            const innerR = 290;
            const outerR = i % 3 === 0 ? 305 : 298;

            const line = document.createElementNS('http://www.w3.org/2000/svg', 'line');
            line.setAttribute('x1', cx + Math.cos(angle) * innerR);
            line.setAttribute('y1', cy + Math.sin(angle) * innerR);
            line.setAttribute('x2', cx + Math.cos(angle) * outerR);
            line.setAttribute('y2', cy + Math.sin(angle) * outerR);
            line.setAttribute('stroke', 'rgba(197, 160, 89, 0.2)');
            line.setAttribute('stroke-width', '0.5');
            svg.appendChild(line);
        }

        runeRing.appendChild(svg);
    }

    // ============================================================
    // Animation styles injected at runtime
    // ============================================================
    const style = document.createElement('style');
    style.textContent = `
        @keyframes floatParticle {
            0% { transform: translate(0, 0) rotate(0deg); opacity: 0; }
            8% { opacity: 1; }
            92% { opacity: 1; }
            100% { transform: translate(${20 + Math.random() * 30}px, -${150 + Math.random() * 100}px) rotate(${180 + Math.random() * 180}deg); opacity: 0; }
        }
        .background-container {
            transition: transform 0.15s cubic-bezier(0.2, 0.8, 0.2, 1);
            will-change: transform;
        }
        .login-card, .game-logo {
            will-change: transform;
        }
    `;
    document.head.appendChild(style);

    // ============================================================
    // Show Result Message helper
    // ============================================================
    function showResult(message, type = 'info') {
        resultMessage.textContent = message;
        resultMessage.className = `result-message ${type}`;
        resultMessage.style.display = 'block';
    }

    function hideResult() {
        resultMessage.style.display = 'none';
    }

    // ============================================================
    // Form Interactions
    // ============================================================
    const loginForm = document.getElementById('loginForm');
    if (loginForm) {
        loginForm.addEventListener('submit', (e) => {
            e.preventDefault();
            const btn = document.getElementById('loginBtn');
            const btnText = btn.querySelector('.btn-text');
            const originalText = btnText.textContent;

            const username = document.getElementById('username').value;
            const password = document.getElementById('password').value;

            if (!username || !password) {
                showResult('Please enter username and password.', 'error');
                return;
            }

            btnText.textContent = 'ESTABLISHING HANDSHAKE...';
            btn.classList.add('loading');
            showResult('Connecting to Faldoran Prime servers...', 'info');

            // Simulate server authentication
            setTimeout(() => {
                btnText.textContent = 'ACCESS GRANTED';
                btn.style.background = 'linear-gradient(135deg, #3d7a3d 0%, #4caf50 50%, #81c784 100%)';
                btn.style.boxShadow = '0 0 30px rgba(76, 175, 80, 0.4)';
                showResult('Welcome back, Traveller. Preparing your world...', 'success');

                setTimeout(() => {
                    // Reset
                    btnText.textContent = originalText;
                    btn.style.background = '';
                    btn.style.boxShadow = '';
                    btn.classList.remove('loading');
                    hideResult();
                }, 2500);
            }, 2000);
        });
    }

    const createBtn = document.getElementById('createAccountBtn');
    if (createBtn) {
        createBtn.addEventListener('click', () => {
            showResult('Opening Identity Manifest... (Account Creation)', 'info');
            setTimeout(() => hideResult(), 3000);
        });
    }

    // ============================================================
    // Input focus enhancement — glow effect on card
    // ============================================================
    const inputs = document.querySelectorAll('.input-wrapper input');
    inputs.forEach(input => {
        input.addEventListener('focus', () => {
            const card = document.querySelector('.card-content');
            if (card) {
                card.style.borderTopColor = 'rgba(197, 160, 89, 0.4)';
                card.style.boxShadow = `
                    0 30px 80px -12px rgba(0, 0, 0, 0.9),
                    0 4px 20px rgba(0, 0, 0, 0.5),
                    inset 0 1px 0 rgba(255, 255, 255, 0.04),
                    inset 0 0 40px rgba(197, 160, 89, 0.05),
                    0 0 40px rgba(197, 160, 89, 0.06)
                `;
            }
        });
        input.addEventListener('blur', () => {
            const card = document.querySelector('.card-content');
            if (card) {
                card.style.borderTopColor = '';
                card.style.boxShadow = '';
            }
        });
    });
});
