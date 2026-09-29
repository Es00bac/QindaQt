(() => {
  const navToggle = document.querySelector('.nav-toggle');
  const nav = document.querySelector('.site-nav');
  navToggle?.addEventListener('click', () => {
    const open = navToggle.getAttribute('aria-expanded') !== 'true';
    navToggle.setAttribute('aria-expanded', String(open));
    nav?.classList.toggle('is-open', open);
  });
  nav?.querySelectorAll('a').forEach(link => link.addEventListener('click', () => {
    navToggle?.setAttribute('aria-expanded', 'false');
    nav.classList.remove('is-open');
  }));

  const descriptions = {
    separate: 'Both applications are independent windows. Group only the ones you choose.',
    split: 'A shared frame holds two applications side by side. Drag the divider to give either one more space.',
    tabs: 'Drop in the middle to make tabbed pages inside one container. Switch pages in the shared title row.'
  };
  const demo = document.querySelector('.demo-desktop');
  const demoDescription = document.querySelector('#mode-description');
  const penguLine = document.querySelector('#pengu-line');
  const penguTip = document.querySelector('#pengu-tip');
  const penguNotes = {
    separate: 'Two independent windows. Nothing groups until you choose it.',
    split: 'Drag to an edge: the windows share a frame and keep working side by side.',
    tabs: 'Drop in the middle: each app gets a page in the same window.'
  };
  document.querySelectorAll('[data-window-mode]').forEach(button => {
    button.addEventListener('click', () => {
      document.querySelectorAll('[data-window-mode]').forEach(item => item.setAttribute('aria-pressed', String(item === button)));
      demo.dataset.mode = button.dataset.windowMode;
      demoDescription.textContent = descriptions[button.dataset.windowMode];
      penguLine.textContent = penguNotes[button.dataset.windowMode];
      penguTip.dataset.mood = button.dataset.windowMode;
      penguTip.querySelector('img').src = button.dataset.windowMode === 'tabs'
        ? 'assets/images/cyber-pengu-ducke-react.webp'
        : button.dataset.windowMode === 'split'
          ? 'assets/images/cyber-pengu-ducke-point.webp'
          : 'assets/images/cyber-pengu-ducke.webp';
    });
  });
  document.querySelectorAll('[data-demo-tab]').forEach(button => {
    button.addEventListener('click', () => {
      demo.dataset.tab = button.dataset.demoTab;
      document.querySelectorAll('[data-demo-tab]').forEach(item =>
        item.setAttribute('aria-pressed', String(item === button)));
    });
  });

  const layoutImage = document.querySelector('#layout-image');
  const layoutName = document.querySelector('#layout-name');
  const layoutDetail = document.querySelector('#layout-detail');
  const layoutKind = document.querySelector('#layout-kind');
  document.querySelectorAll('[data-layout]').forEach(button => {
    button.addEventListener('click', () => {
      document.querySelectorAll('[data-layout]').forEach(item => {
        item.classList.toggle('active', item === button);
        item.setAttribute('aria-pressed', String(item === button));
      });
      layoutImage.src = `assets/images/${button.dataset.image}`;
      layoutImage.alt = `${button.dataset.kind === 'Live capture' ? 'Live QindaQt capture of' : 'Rendered preview of'} the ${button.dataset.title} layout`;
      layoutName.textContent = button.dataset.title;
      layoutDetail.textContent = button.dataset.detail;
      layoutKind.textContent = button.dataset.kind === 'Live capture' ? 'Captured in QindaQt' : 'Shell render · preview';
    });
  });

  const cornerImage = document.querySelector('#corner-press-image');
  const cornerName = document.querySelector('#corner-press-name');
  const cornerRadius = document.querySelector('#corner-press-radius');
  document.querySelectorAll('[data-corner-image]').forEach(button => {
    button.addEventListener('click', () => {
      document.querySelectorAll('[data-corner-image]').forEach(item => {
        item.classList.toggle('active', item === button);
        item.setAttribute('aria-pressed', String(item === button));
      });
      cornerImage.src = `assets/images/${button.dataset.cornerImage}`;
      cornerImage.alt = `Live Corner Bar desktop in the ${button.dataset.cornerName} theme`;
      cornerName.textContent = button.dataset.cornerName.toUpperCase();
      cornerRadius.textContent = button.dataset.cornerRadius;
    });
  });

  const themeImage = document.querySelector('#theme-image');
  const themeName = document.querySelector('#theme-name');
  const duckeLine = document.querySelector('#ducke-line');
  const duckeNotes = {
    'Compile Club': 'I brought the fan. He brought the coffee.',
    'Qinda Punk': 'Rooftop duty. Sunglasses mandatory.',
    'Jade Fold': 'The calm one. Even I rest sometimes.',
    'Porcelain Dawn': 'A clean slate for a new day.',
    'Ink Tide': 'Quiet enough to hear the next idea.',
    'Qinda Bliss': 'Hills, blue sky, and no compile queue.'
  };
  document.querySelectorAll('[data-wallpaper]').forEach(button => {
    button.addEventListener('click', () => {
      document.querySelectorAll('[data-wallpaper]').forEach(item => {
        item.classList.toggle('active', item === button);
        item.setAttribute('aria-pressed', String(item === button));
      });
      themeImage.src = `assets/images/${button.dataset.wallpaper}`;
      themeImage.alt = `${button.dataset.label} QindaQt wallpaper`;
      themeName.textContent = button.dataset.label;
      duckeLine.textContent = duckeNotes[button.dataset.label];
      duckeLine.closest('.mascot-tip')?.querySelector('img')?.setAttribute(
        'src', button.dataset.label === 'Compile Club'
          ? 'assets/images/cyber-pengu-ducke-point.webp'
          : 'assets/images/cyber-pengu-ducke-react.webp');
    });
  });

  const desktopTour = document.querySelector('#desktop-tour-video');
  document.querySelectorAll('[data-tour-time]').forEach(button => {
    button.addEventListener('click', () => {
      if (!desktopTour) return;
      desktopTour.currentTime = Number(button.dataset.tourTime);
      desktopTour.play();
      desktopTour.scrollIntoView({ block: 'center' });
    });
  });
})();
