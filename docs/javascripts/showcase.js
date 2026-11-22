/* SPDX-License-Identifier: GPL-3.0-or-later */
(() => {
  const showcase = document.querySelector('.gloam-showcase');
  if (!showcase) return;
  const controls = showcase.querySelector('[role="tablist"]');
  const tabs = Array.from(controls.querySelectorAll('[role="tab"]'));
  const select = tab => {
    for (const candidate of tabs) {
      const active = candidate === tab;
      candidate.setAttribute('aria-selected', String(active));
      candidate.tabIndex = active ? 0 : -1;
      document.getElementById(candidate.getAttribute('aria-controls')).hidden = !active;
    }
  };
  for (const tab of tabs) {
    tab.addEventListener('click', () => select(tab));
    tab.addEventListener('keydown', event => {
      let next = tabs.indexOf(tab);
      if (event.key === 'ArrowRight') next = (next + 1) % tabs.length;
      else if (event.key === 'ArrowLeft') next = (next + tabs.length - 1) % tabs.length;
      else if (event.key === 'Home') next = 0;
      else if (event.key === 'End') next = tabs.length - 1;
      else return;
      event.preventDefault();
      select(tabs[next]);
      tabs[next].focus();
    });
  }
  select(tabs[0]);
  controls.hidden = false;

  const dialog = document.querySelector('#gloam-preview');
  if (!dialog || typeof dialog.showModal !== 'function') return;
  const image = dialog.querySelector('img');
  image.addEventListener('load', () => {
    image.style.width = `${Math.min(image.naturalWidth * 2, 1200)}px`;
  });
  for (const link of showcase.querySelectorAll('[data-gloam-preview]')) {
    link.addEventListener('click', event => {
      if (event.ctrlKey || event.metaKey || event.shiftKey || event.altKey) return;
      event.preventDefault();
      image.alt = link.querySelector('img').alt;
      image.src = link.href;
      dialog.showModal();
    });
  }
  dialog.addEventListener('click', event => {
    if (event.target !== dialog) return;
    const box = dialog.getBoundingClientRect();
    if (event.clientX < box.left || event.clientX > box.right ||
        event.clientY < box.top || event.clientY > box.bottom) dialog.close();
  });
})();
