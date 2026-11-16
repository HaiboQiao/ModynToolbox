// Tool-owned interaction default; the host never injects this module.
// Wheel scrolls the page/nearest scroll container, never parameter values.
document.addEventListener('wheel', event => {
  if (event.ctrlKey || !event.target.closest?.('input[type="number"],input[type="range"],select')) return;
  event.preventDefault();
  const unit = event.deltaMode === 1 ? 20 : event.deltaMode === 2 ? innerHeight : 1;
  const dx = event.deltaX * unit, dy = event.deltaY * unit;
  for (let node = event.target.parentElement; node; node = node.parentElement) {
    const style = getComputedStyle(node);
    const y = /(auto|scroll)/.test(style.overflowY) && node.scrollHeight > node.clientHeight &&
      (dy < 0 ? node.scrollTop > 0 : node.scrollTop < node.scrollHeight - node.clientHeight);
    const x = /(auto|scroll)/.test(style.overflowX) && node.scrollWidth > node.clientWidth &&
      (dx < 0 ? node.scrollLeft > 0 : node.scrollLeft < node.scrollWidth - node.clientWidth);
    if (y || x) { node.scrollBy(dx, dy); return; }
  }
  document.scrollingElement?.scrollBy(dx, dy);
}, {capture:true, passive:false});
