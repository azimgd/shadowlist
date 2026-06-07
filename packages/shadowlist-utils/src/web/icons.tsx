import { colors } from './theme';

// SF-Symbol-style icons as inline SVG; tinted via the `color` prop.

type IconProps = {
  size?: number;
  color?: string;
  strokeWidth?: number;
};

type Direction = 'up' | 'down' | 'left' | 'right';

const CHEVRON_POINTS: Record<Direction, string> = {
  right: '9 6 15 12 9 18',
  left: '15 6 9 12 15 18',
  up: '6 15 12 9 18 15',
  down: '6 9 12 15 18 9',
};

export const Chevron = ({
  size = 17,
  color = colors.label,
  strokeWidth = 2,
  direction = 'right',
}: IconProps & { direction?: Direction }) => (
  <svg width={size} height={size} viewBox="0 0 24 24" fill="none" aria-hidden>
    <polyline
      points={CHEVRON_POINTS[direction]}
      stroke={color}
      strokeWidth={strokeWidth}
      strokeLinecap="round"
      strokeLinejoin="round"
    />
  </svg>
);

// Viewfinder / locate target — the "scroll to random" affordance.
export const Viewfinder = ({ size = 22, color = colors.label, strokeWidth = 2 }: IconProps) => (
  <svg width={size} height={size} viewBox="0 0 24 24" fill="none" aria-hidden>
    <circle cx="12" cy="12" r="6" stroke={color} strokeWidth={strokeWidth} />
    <line x1="12" y1="1.5" x2="12" y2="4.5" stroke={color} strokeWidth={strokeWidth} strokeLinecap="round" />
    <line x1="12" y1="19.5" x2="12" y2="22.5" stroke={color} strokeWidth={strokeWidth} strokeLinecap="round" />
    <line x1="1.5" y1="12" x2="4.5" y2="12" stroke={color} strokeWidth={strokeWidth} strokeLinecap="round" />
    <line x1="19.5" y1="12" x2="22.5" y2="12" stroke={color} strokeWidth={strokeWidth} strokeLinecap="round" />
    <circle cx="12" cy="12" r="1.4" fill={color} />
  </svg>
);

// Filled folder, tinted with the accent (Files-app style).
export const Folder = ({ size = 18, color = colors.accent }: IconProps) => (
  <svg width={size} height={size} viewBox="0 0 24 24" fill="none" aria-hidden>
    <path
      d="M3 7.5C3 6.4 3.9 5.5 5 5.5H9L11 7.5H19C20.1 7.5 21 8.4 21 9.5V17C21 18.1 20.1 19 19 19H5C3.9 19 3 18.1 3 17V7.5Z"
      fill={color}
    />
  </svg>
);

// Outlined document with a folded corner.
export const Doc = ({ size = 18, color = colors.secondaryLabel, strokeWidth = 1.7 }: IconProps) => (
  <svg width={size} height={size} viewBox="0 0 24 24" fill="none" aria-hidden>
    <path
      d="M7 3.5H13L18 8.5V19.5C18 20.05 17.55 20.5 17 20.5H7C6.45 20.5 6 20.05 6 19.5V4.5C6 3.95 6.45 3.5 7 3.5Z"
      stroke={color}
      strokeWidth={strokeWidth}
      strokeLinejoin="round"
    />
    <path d="M13 3.5V8.5H18" stroke={color} strokeWidth={strokeWidth} strokeLinejoin="round" />
  </svg>
);

// Three-line reorder grip.
export const Grip = ({ size = 20, color = colors.tertiaryLabel, strokeWidth = 1.75 }: IconProps) => (
  <svg width={size} height={size} viewBox="0 0 24 24" fill="none" aria-hidden>
    <line x1="5" y1="8" x2="19" y2="8" stroke={color} strokeWidth={strokeWidth} strokeLinecap="round" />
    <line x1="5" y1="12" x2="19" y2="12" stroke={color} strokeWidth={strokeWidth} strokeLinecap="round" />
    <line x1="5" y1="16" x2="19" y2="16" stroke={color} strokeWidth={strokeWidth} strokeLinecap="round" />
  </svg>
);

// Appearance toggle — a ring with one half filled.
export const HalfCircle = ({ size = 22, color = colors.label, strokeWidth = 2 }: IconProps) => (
  <svg width={size} height={size} viewBox="0 0 24 24" fill="none" aria-hidden>
    <circle cx="12" cy="12" r="7.5" stroke={color} strokeWidth={strokeWidth} />
    <path d="M12 4.5 A7.5 7.5 0 0 1 12 19.5 Z" fill={color} />
  </svg>
);

// Ring with a diagonal slash — an "off / offline" affordance.
export const CircleSlash = ({ size = 22, color = colors.label, strokeWidth = 2 }: IconProps) => (
  <svg width={size} height={size} viewBox="0 0 24 24" fill="none" aria-hidden>
    <circle cx="12" cy="12" r="7.5" stroke={color} strokeWidth={strokeWidth} />
    <line x1="6.7" y1="17.3" x2="17.3" y2="6.7" stroke={color} strokeWidth={strokeWidth} strokeLinecap="round" />
  </svg>
);

// Notification bell.
export const Bell = ({ size = 22, color = colors.label, strokeWidth = 2 }: IconProps) => (
  <svg width={size} height={size} viewBox="0 0 24 24" fill="none" aria-hidden>
    <path
      d="M18 8a6 6 0 0 0-12 0c0 7-3 9-3 9h18s-3-2-3-9"
      stroke={color}
      strokeWidth={strokeWidth}
      strokeLinecap="round"
      strokeLinejoin="round"
    />
    <path
      d="M13.73 21a2 2 0 0 1-3.46 0"
      stroke={color}
      strokeWidth={strokeWidth}
      strokeLinecap="round"
      strokeLinejoin="round"
    />
  </svg>
);

// Globe — a ring with a meridian and an equator.
export const Globe = ({ size = 22, color = colors.label, strokeWidth = 2 }: IconProps) => (
  <svg width={size} height={size} viewBox="0 0 24 24" fill="none" aria-hidden>
    <circle cx="12" cy="12" r="9.5" stroke={color} strokeWidth={strokeWidth} />
    <line x1="2.5" y1="12" x2="21.5" y2="12" stroke={color} strokeWidth={strokeWidth} />
    <path
      d="M12 2.5a15 15 0 0 1 4 9.5 15 15 0 0 1-4 9.5 15 15 0 0 1-4-9.5 15 15 0 0 1 4-9.5z"
      stroke={color}
      strokeWidth={strokeWidth}
      strokeLinejoin="round"
    />
  </svg>
);

// Color swatch — a rounded tile, for theming.
export const Swatch = ({ size = 22, color = colors.label }: IconProps) => (
  <svg width={size} height={size} viewBox="0 0 24 24" fill="none" aria-hidden>
    <rect x="5" y="5" width="14" height="14" rx="4" fill={color} />
  </svg>
);

// Up-arrow for the message send button.
export const ArrowUp = ({ size = 20, color = colors.label, strokeWidth = 2.2 }: IconProps) => (
  <svg width={size} height={size} viewBox="0 0 24 24" fill="none" aria-hidden>
    <line x1="12" y1="19" x2="12" y2="6" stroke={color} strokeWidth={strokeWidth} strokeLinecap="round" />
    <polyline
      points="6 11 12 5 18 11"
      stroke={color}
      strokeWidth={strokeWidth}
      strokeLinecap="round"
      strokeLinejoin="round"
    />
  </svg>
);
