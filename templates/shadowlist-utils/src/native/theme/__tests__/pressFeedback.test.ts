/*
 * The themes import react-native, which does not load under Node. The mock gives them the
 * Platform they read, and each test sets the OS it runs on.
 */
jest.mock('react-native', () => ({ Platform: { OS: 'ios' } }), {
  virtual: true,
});

import { Platform } from 'react-native';
import { pressRippleFor } from '../../internal/pressRipple';
import { createTheme } from '../createTheme';
import { darkTheme } from '../darkTheme';
import { lightTheme } from '../lightTheme';

const platform = Platform as { OS: string };

describe('the platform tokens', () => {
  it('default to capital captions, chevrons and the pressed highlight', () => {
    for (const theme of [lightTheme, darkTheme]) {
      expect(theme.grouped.captionCase).toBe('upper');
      expect(theme.grouped.chevron).toBe(true);
      expect(theme.pressFeedback).toBe('highlight');
    }
  });

  it('merge from a partial override and keep the other grouped metrics', () => {
    const theme = createTheme(lightTheme, {
      grouped: { captionCase: 'none', chevron: false },
      pressFeedback: 'ripple',
    });
    expect(theme.grouped).toEqual({
      ...lightTheme.grouped,
      captionCase: 'none',
      chevron: false,
    });
    expect(theme.pressFeedback).toBe('ripple');
  });
});

describe('pressRippleFor', () => {
  const rippleTheme = createTheme(lightTheme, { pressFeedback: 'ripple' });

  afterEach(() => {
    platform.OS = 'ios';
  });

  it('gives no ripple to a highlight theme on Android', () => {
    platform.OS = 'android';
    expect(pressRippleFor(lightTheme)).toBeUndefined();
  });

  it('gives a ripple theme a ripple in the fill color on Android', () => {
    platform.OS = 'android';
    const theme = createTheme(lightTheme, { pressFeedback: 'ripple' });
    expect(pressRippleFor(theme)).toEqual({ color: lightTheme.colors.fill });
    expect(pressRippleFor(theme)).toBe(pressRippleFor(theme));
  });

  it('falls back to the highlight off Android', () => {
    expect(pressRippleFor(rippleTheme)).toBeUndefined();
  });
});
