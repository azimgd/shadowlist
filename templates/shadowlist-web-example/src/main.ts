import { loadShadowListKit } from 'shadowlist-wasm';

import { SCREENS } from './Screens.js';
import { Settings } from './Settings.js';

/*
 * One page per route. The links at the top switch routes and keep the other query parameters.
 */
function buildNavigation(nav: HTMLElement): void {
  for (const route of Object.keys(SCREENS)) {
    const params = new URLSearchParams(location.search);
    params.set('SLRoute', route);
    const link = document.createElement('a');
    link.href = `?${params}`;
    link.textContent = route;
    if (route === Settings.route) link.className = 'active';
    nav.append(link);
  }
}

await loadShadowListKit();
buildNavigation(document.getElementById('routes')!);
const screen = SCREENS[Settings.route] ?? SCREENS.Feed!;
const list = screen(document.getElementById('screen')!);
// The page exposes the list for driving it from the console or a test.
Object.assign(window, { list });
