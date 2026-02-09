document.addEventListener('DOMContentLoaded', () => {
    const particlesContainer = document.getElementById('particles');
    const particleCount = 60;
    const loginCard = document.querySelector('.login-card');
    const logo = document.querySelector('.game-logo');

    // Parallax Effect
    document.addEventListener('mousemove', (e) => {
        const moveX = (e.clientX - window.innerWidth / 2) * 0.01;
        const moveY = (e.clientY - window.innerHeight / 2) * 0.01;

        if (loginCard) {
            loginCard.style.transform = `translate(${moveX}px, ${moveY}px)`;
        }
        if (logo) {
            logo.style.transform = `translate(${moveX * -1.5}px, ${moveY * -1.5}px)`;
        }

        const background = document.querySelector('.background-container');
        if (background) {
            // Slight counter-movement for background to increase depth
            background.style.transform = `scale(1.05) translate(${moveX * -0.5}px, ${moveY * -0.5}px)`;
        }
    });

    // Create background particles
    for (let i = 0; i < particleCount; i++) {
        const particle = document.createElement('div');
        particle.className = 'particle';

        const posX = Math.random() * 100;
        const posY = Math.random() * 100;
        const delay = Math.random() * 10;
        const duration = 15 + Math.random() * 25;
        const size = 1 + Math.random() * 4;
        const opacity = 0.05 + Math.random() * 0.3;

        particle.style.cssText = `
            position: absolute;
            left: ${posX}%;
            top: ${posY}%;
            width: ${size}px;
            height: ${size}px;
            background: rgba(255, 255, 255, ${opacity});
            border-radius: 50%;
            filter: blur(1px);
            animation: floatParticle ${duration}s infinite linear;
            animation-delay: -${delay}s;
            pointer-events: none;
        `;

        particlesContainer.appendChild(particle);
    }

    // Add particle animation style with slight rotation
    const style = document.createElement('style');
    style.textContent = `
        @keyframes floatParticle {
            0% { transform: translate(0, 0) rotate(0deg); opacity: 0; }
            10% { opacity: 1; }
            90% { opacity: 1; }
            100% { transform: translate(30px, -200px) rotate(360deg); opacity: 0; }
        }
        .background-container {
            transition: transform 0.2s cubic-bezier(0.2, 0.8, 0.2, 1);
            will-change: transform;
        }
        .login-card, .game-logo {
            transition: transform 0.2s cubic-bezier(0.2, 0.8, 0.2, 1);
            will-change: transform;
        }
    `;
    document.head.appendChild(style);

    // Form Interactions
    const loginForm = document.getElementById('loginForm');
    if (loginForm) {
        loginForm.addEventListener('submit', (e) => {
            e.preventDefault();
            const btn = loginForm.querySelector('.primary-btn');
            const btnText = btn.querySelector('.btn-text');
            const originalText = btnText.textContent;

            btnText.textContent = 'ESTABLISHING HANDSHAKE...';
            btn.style.opacity = '0.7';
            btn.style.pointerEvents = 'none';

            // Simulate server authentication
            setTimeout(() => {
                btnText.textContent = 'ACCESS GRANTED';
                btn.style.background = 'linear-gradient(135deg, #4caf50 0%, #81c784 100%)';
                btn.style.boxShadow = '0 0 30px rgba(76, 175, 80, 0.5)';

                setTimeout(() => {
                    alert('Faldoran Prime: Connection Handshake Complete. Welcome, Traveller.');
                    btnText.textContent = originalText;
                    btn.style.background = '';
                    btn.style.boxShadow = '';
                    btn.style.opacity = '1';
                    btn.style.pointerEvents = 'auto';
                }, 1000);
            }, 2000);
        });
    }

    const createBtn = document.getElementById('createAccountBtn');
    if (createBtn) {
        createBtn.addEventListener('click', () => {
            alert('Navigating to Identity Manifest... (Account Creation Mockup)');
        });
    }
});
