(() => {
  const sources = {
    virtual: { title: 'Virtual input', detail: "Choose a QindaQt virtual sink as an app's output and give that app its own strip." },
    mic: { title: 'Hardware input', detail: 'A microphone or other capture device feeds a hardware strip, with a pin available by device name.' },
    peer: { title: 'Received stream', detail: 'A manually configured VBAN stereo receive stream appears as a source when enabled and active.' }
  };
  const status = document.querySelector('#route-status');
  const sourceTitle = document.querySelector('#route-source');
  if (!status || !sourceTitle) return;
  const selected = new Set(['A1', 'B1']);
  let source = 'virtual';
  const paint = () => {
    const buses = [...selected].sort();
    const label = buses.length ? buses.join(' + ') : 'no bus selected';
    sourceTitle.textContent = sources[source].title;
    document.querySelector('#route-source-detail').textContent = sources[source].detail;
    document.querySelector('#route-output').textContent = label;
    document.querySelector('#route-output-detail').textContent = buses.length
      ? 'Each selected bus gets a separate send with its own gain.'
      : 'Choose at least one bus to illustrate a route.';
    status.textContent = `${sources[source].title} → ${label}`;
  };
  document.querySelectorAll('[data-source]').forEach(button => button.addEventListener('click', () => {
    source = button.dataset.source;
    document.querySelectorAll('[data-source]').forEach(item => {
      item.classList.toggle('active', item === button);
      item.setAttribute('aria-pressed', String(item === button));
    });
    paint();
  }));
  document.querySelectorAll('[data-bus]').forEach(button => button.addEventListener('click', () => {
    if (selected.has(button.dataset.bus)) selected.delete(button.dataset.bus);
    else selected.add(button.dataset.bus);
    button.setAttribute('aria-pressed', String(selected.has(button.dataset.bus)));
    paint();
  }));
})();
