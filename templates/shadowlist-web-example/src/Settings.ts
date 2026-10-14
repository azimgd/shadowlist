/*
 * Query parameters, the same names the other examples read as launch arguments:
 * ?SLRoute=Chat&SLCount=1000.
 */
export class Settings {
  static readonly route: string =
    new URLSearchParams(location.search).get('SLRoute') ?? 'Feed';
  static readonly count: number = Math.max(
    1,
    Number(new URLSearchParams(location.search).get('SLCount')) || 1000
  );
}
