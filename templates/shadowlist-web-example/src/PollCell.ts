import { ShadowListKitListCell } from 'shadowlist-wasm';
import { pollPercent } from 'shadowlist-wasm/utils';
import type { PollOption } from './PollOption.js';

/*
 * The app controls the vote. A press reports the option and the next configure supplies it.
 */
export class PollCell extends ShadowListKitListCell {
  static readonly IDENTIFIER = 'poll';
  private readonly button = document.createElement('button');
  private option: PollOption | null = null;
  onPressItem: ((option: PollOption) => void) | null = null;

  constructor() {
    super(PollCell.IDENTIFIER);
    this.view.className = 'poll-cell';
    this.button.type = 'button';
    this.button.addEventListener('click', () => {
      if (this.option) this.onPressItem?.(this.option);
    });
    this.view.append(this.button);
  }

  configure(
    option: PollOption,
    totalVotes: number,
    selectedKey: string | undefined
  ): void {
    this.option = option;
    this.button.textContent = `${option.label} · ${pollPercent(option.votes, totalVotes)}% · ${option.votes} votes`;
    this.button.setAttribute(
      'aria-pressed',
      String(option.key === selectedKey)
    );
    this.button.disabled = this.onPressItem === null;
  }
}
