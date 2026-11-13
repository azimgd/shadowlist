import { SINGLE_IMAGE_HEIGHT, getSingleImageFrame } from '../singleImageFrame';

/*
 * Sizes a vector file cannot hold, since JSON has no infinity. The core's
 * tests_utils_feed_layout.cpp checks the same sizes.
 */
describe('getSingleImageFrame', () => {
  it('frames two infinite sides by the fixed height', () => {
    expect(
      getSingleImageFrame({ uri: 'image', width: Infinity, height: Infinity })
    ).toEqual({ height: SINGLE_IMAGE_HEIGHT });
  });

  it('frames a side that is not a number by the fixed height', () => {
    expect(
      getSingleImageFrame({ uri: 'image', width: NaN, height: 600 })
    ).toEqual({ height: SINGLE_IMAGE_HEIGHT });
  });
});
