export function clampFraction(fraction: number): number {
  return Math.max(0, Math.min(1, Number.isFinite(fraction) ? fraction : 0));
}
