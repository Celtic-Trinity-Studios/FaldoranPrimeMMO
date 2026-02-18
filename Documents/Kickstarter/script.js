/* ─────────────────────────────────────────────
   FALDORAN PRIME — KICKSTARTER JS
   Minimal enhancements: scroll reveals + smooth anchor
   ───────────────────────────────────────────── */

document.addEventListener('DOMContentLoaded', () => {

  /* ═══ SCROLL-REVEAL FOR CARDS & SECTIONS ═══ */
  const revealTargets = document.querySelectorAll(
    '.pillar-card, .reward-card, .feature-block, .stretch-goal, .tech-item, .faq-item'
  );

  const revealObserver = new IntersectionObserver((entries) => {
    entries.forEach(entry => {
      if (entry.isIntersecting) {
        entry.target.classList.add('revealed');
        revealObserver.unobserve(entry.target);
      }
    });
  }, { threshold: 0.12, rootMargin: '0px 0px -40px 0px' });

  revealTargets.forEach(el => {
    el.style.opacity = '0';
    el.style.transform = 'translateY(28px)';
    el.style.transition = 'opacity .6s cubic-bezier(.22,1,.36,1), transform .6s cubic-bezier(.22,1,.36,1)';
    revealObserver.observe(el);
  });

  // Apply reveal
  const style = document.createElement('style');
  style.textContent = `.revealed { opacity: 1 !important; transform: translateY(0) !important; }`;
  document.head.appendChild(style);

  /* ═══ STAGGERED REVEAL FOR GRIDS ═══ */
  const grids = document.querySelectorAll('.pillars-grid, .rewards-grid, .tech-grid');
  grids.forEach(grid => {
    const items = grid.children;
    Array.from(items).forEach((item, i) => {
      item.style.transitionDelay = `${i * 80}ms`;
    });
  });

  /* ═══ SMOOTH ANCHOR SCROLLING ═══ */
  document.querySelectorAll('a[href^="#"]').forEach(anchor => {
    anchor.addEventListener('click', (e) => {
      const target = document.querySelector(anchor.getAttribute('href'));
      if (target) {
        e.preventDefault();
        target.scrollIntoView({ behavior: 'smooth', block: 'start' });
      }
    });
  });

});
