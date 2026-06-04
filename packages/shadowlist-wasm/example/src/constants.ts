import type { FeedElement } from './FeedElement';
import type { NestedElement, NestedElementChild } from './NestedElement';
import type { MasonryElement } from './MasonryElement';
import type { ContactElement } from './ContactElement';

export const AVATAR_COLORS = [
  '#FF6B6B', '#4ECDC4', '#45B7D1', '#FFA07A', '#98D8C8',
  '#F7DC6F', '#BB8FCE', '#85C1E2', '#F8B195', '#C06C84',
];

export const IMAGES = [
  'https://apod.nasa.gov/apod/image/2507/Pleiades_Kayali_2560.jpg',
  'https://apod.nasa.gov/apod/image/2507/Trifid2048.jpg',
  'https://apod.nasa.gov/apod/image/2507/LDN1251gualco2048.JPG',
  'https://apod.nasa.gov/apod/image/2507/LUA_JULHO_25_2048.jpg',
  'https://apod.nasa.gov/apod/image/2507/ant_hubble_1072.jpg',
  'https://apod.nasa.gov/apod/image/2507/Ngc2685_Thrun_960.jpg',
  'https://apod.nasa.gov/apod/image/2507/HebesChasma_esa_960.jpg',
  'https://apod.nasa.gov/apod/image/2507/Rosette_Decam_4000.jpg',
  'https://apod.nasa.gov/apod/image/2507/noirlab2522a_3i.jpg',
  'https://apod.nasa.gov/apod/image/2507/ISSMeetsSaturn3.jpg',
  'https://apod.nasa.gov/apod/image/2507/M6.jpg',
  'https://apod.nasa.gov/apod/image/1103/lroc_wac_nearside.jpg',
  'https://apod.nasa.gov/apod/image/2507/CatsPaw_Webb_1822.jpg',
  'https://apod.nasa.gov/apod/image/2507/DoubleSN_ESO_3000.jpg',
  'https://apod.nasa.gov/apod/image/2507/MeteorMilkyWay_Rice_2000.jpg',
  'https://apod.nasa.gov/apod/image/2507/SaturnJuly18_2025TitanShadowTransit1200.png',
  'https://apod.nasa.gov/apod/image/2507/KCG2021_08_11_Pano_Elafonisi_met_fin-CCMZ_1500px.png',
  'https://apod.nasa.gov/apod/image/2507/oc_ls_2025.jpg',
  'https://apod.nasa.gov/apod/image/2507/LightningVolcano_Montufar_3000.jpg',
  'https://apod.nasa.gov/apod/image/2501/AtlasParanal_Kurak_2000.jpg',
];

export const SAMPLE_TEXTS = [
  'The Time Machine stood in the corner. It was made of nickel, ivory, and some transparent crystalline substance. And now I must be explicit, for this that follows is an absolutely unaccountable thing. He took one of the small octagonal tables that were scattered about the room, and set it in front of the fire, with two legs on the hearthrug. On this table he placed the mechanism. Then he drew up a chair and sat down.\n\nThe only other object on the table was a small shaded lamp, the bright light of which fell upon the model. There were also perhaps a dozen candles about, two in brass candlesticks upon the mantel and several in sconces, so that the room was brilliantly illuminated.\n\nI sat in a low arm-chair nearest the fire, and I drew this forward so as to be almost between the Time Traveller and the fireplace. Filby sat behind him, looking over his shoulder.',
  'I was in the dark—trapped. So the Morlocks thought. At that I chuckled gleefully.',
  'As I walked I was watching for every impression that could possibly help to explain the condition of ruinous splendour in which I found the world. The buildings and machines were in decay. Here and there I saw figures moving, but they seemed to move with such languor that I could distinguish nothing clearly.\n\nI felt faint and confused. So, presently, I left them, meaning to go back to the house and lie down. But I was strangely agitated. The world seemed to spin around me and I saw the sun hopping swiftly across the sky, leaping it every minute, and every minute marking a day.',
  'The whole world will be intelligent, educated, and co-operating; things will move faster and faster towards the subjugation of Nature. In the end, wisely and carefully we shall readjust the balance of animal and vegetable life.',
  'So far as I could see, all the world displayed the same exuberant richness as the Thames valley. From every hill I climbed I saw the same abundance of splendid buildings, endlessly varied in material and style.',
  'The silence grew more oppressive, and I shivered.',
  'I cannot describe the strange sensations of time travelling. They are excessively unpleasant. There is a feeling exactly like that one has upon a switchback, of a helpless headlong motion.',
  'The moon was setting, and the dying moonlight and the first pallor of dawn were mingled in a ghastly half-light. The bushes were inky black, the ground a sombre grey, the sky colourless and cheerless. And up the hill I thought I could see ghosts. There several times, as I scanned the slope, I saw white figures.',
  'I had got to such a low estimate of her kind that I did not expect any gratitude from her. In that, however, I was wrong. This, I must remind you, was my speculation at the time.',
  'Above me towered the sphinx, upon the bronze pedestal, white, shining, leprous, in the light of the rising moon. It seemed to smile in mockery of my dismay.',
  'Nature never appeals to intelligence until habit and instinct are useless.',
  'I felt hopelessly cut off from my own kind—a strange animal in an unknown world. I must have been a nightmare to them. I had wild visions of marching down the hall and slaying the monster. The great hall was dark and silent as the grave. I tried to light a match, but my hands were trembling too much.\n\nI stood in the dark for a long while, thinking of my desperate venture. At last, I pushed on through the darkness, feeling my way carefully. My hands came upon something round and smooth. It was a pillar. I worked my way round it, and came to a large open space.\n\nLooking upward, I saw a faint glimmer of light. It was the daylight filtering down through a hole in the roof. I felt a sense of relief. There was a way out, if I could only reach it.',
  'The calm of evening was upon the world as I emerged from the great hall, and the scene was lit by the warm glow of the setting sun.',
  'You see, I had always anticipated that the people of the year Eight Hundred and Two Thousand odd would be incredibly in front of us in knowledge, art, everything. Instead, I found ruins and decay. The great buildings were crumbling. The once magnificent civilization had fallen into a strange and primitive state.\n\nWhat had happened? I could not understand it. Where was the advancement I had expected? Where were the flying machines and the great inventions? All I saw was decay and simplicity.',
  'I grieved to think how brief the dream of the human intellect had been. It had committed suicide. It had set itself steadfastly towards comfort and ease.',
  'Even through the veil of my confusion the earth seemed very fair. And so my mind came round to the business of stopping. The peculiar risk lay in the possibility of my finding some substance in the space.',
  'Looking at these stars suddenly dwarfed my own troubles.',
  'The stars shone brilliantly, and the night was very cold and still. In the morning there was a white frost, and I found a broken cup near the sundial. For some reason this affected me deeply. I picked it up, feeling a strange connection to whoever had drunk from it last.',
  'I clenched my hands and steadfastly looked into the glaring eyeballs. The machine halted. The dim outlines of a desolate beach grew visible.',
  'It sounds plausible enough tonight, said the Medical Man; but wait until tomorrow. Wait for the common sense of the morning. All these things seemed to me such childish tricks. The Time Machine, if there was such a thing, was a magnificent idea, but wholly impracticable.\n\nWe stared at each other. The thing is not a jest, I said. I have experimental verification. I had been working at it for years, and now I have succeeded. This little affair is only a model.\n\nThen I shall have to ask you to believe. That is all. But I warn you that some of you may find the story difficult to accept. Yet I assure you it is the absolute truth.',
];

export const SECTION_TITLES = [
  'Nebulae & Star Clusters',
  'Distant Galaxies',
  'Our Solar System',
  'Deep Space Phenomena',
  'Cosmic Landscapes',
  'Stellar Evolution',
  'Planetary Features',
  'Astronomical Events',
  'Celestial Wonders',
  'Space Exploration',
];

export const IMAGE_TITLES = [
  'The Pleiades Star Cluster',
  'Trifid Nebula',
  'Dark Nebula LDN 1251',
  'Full Moon in July',
  'The Ant Nebula',
  'Helix Galaxy NGC 2685',
  'Hebes Chasma on Mars',
  'The Rosette Nebula',
  'Spiral Galaxy',
  'ISS Transit Across Saturn',
  'The Butterfly Cluster',
  "Earth's Moon",
  "Cat's Paw Nebula",
  'Double Supernova Remnant',
  'Meteor and Milky Way',
  'Titan Shadow on Saturn',
  'Milky Way Panorama',
  'Aurora Borealis',
  'Lightning Over Volcano',
  'Atlas and Paranal Observatory',
];

export const CHARACTER_NAMES = [
  'Time Traveller',
  'Medical Man',
  'Psychologist',
  'Editor',
  'Journalist',
  'Silent Man',
  'Provincial Mayor',
  'Weena',
  'Filby',
  'Eloi Elder',
  'Morlock Scout',
  'Narrator',
  'Very Young Man',
  'Laboratory Assistant',
  'Hillyer',
];

let uniqueIdCounter = 0;
export function generateUniqueId(): string {
  return `${Date.now()}-${(uniqueIdCounter++).toString(36)}-${Math.random()
    .toString(36)
    .substr(2, 9)}`;
}

export function optimizeImageUrl(originalUrl: string, width: number): string {
  const cleanUrl = originalUrl.replace('https://', '');
  return `https://images.weserv.nl/?url=${cleanUrl}&q=60&w=${width}`;
}

export function generateRandomText(seed: number): string {
  return SAMPLE_TEXTS[seed % SAMPLE_TEXTS.length]!;
}

export function generateOptimizedImageUrl(index: number): string | undefined {
  const shouldHaveImage = index > 0 && index % 5 === 0;
  const isGridImage = index > 0 && index % 10 === 0;

  if (!shouldHaveImage || isGridImage) {
    return undefined;
  }

  const originalImageUrl = IMAGES[index % IMAGES.length]!;
  return optimizeImageUrl(originalImageUrl, 800);
}

export function shouldBeImageGrid(index: number): boolean {
  return index > 0 && index % 10 === 0;
}

export function generateImageGrid(startIndex: number): string[] {
  const imageUrls: string[] = [];
  for (let i = 0; i < 4; i++) {
    const imageIndex = (startIndex + i) % IMAGES.length;
    const originalImageUrl = IMAGES[imageIndex]!;
    imageUrls.push(optimizeImageUrl(originalImageUrl, 400));
  }
  return imageUrls;
}

export function generateFeedElement(index: number): FeedElement {
  const characterName = CHARACTER_NAMES[index % CHARACTER_NAMES.length]!;
  const handle = characterName.toLowerCase().replace(/\s+/g, '');

  const hasMultipleImages = index % 10 === 0;
  const imageCount = hasMultipleImages ? 3 + (index % 2) : 1;

  const imageUrls: string[] = [];
  for (let i = 0; i < imageCount; i++) {
    const imageIndex = (index + i) % IMAGES.length;
    const originalImageUrl = IMAGES[imageIndex]!;
    imageUrls.push(optimizeImageUrl(originalImageUrl, 800));
  }

  return {
    id: generateUniqueId(),
    username: characterName,
    handle: `@${handle}`,
    text: SAMPLE_TEXTS[index % SAMPLE_TEXTS.length]!,
    imageUrls,
    timestamp: `${Math.floor(Math.random() * 24)}h`,
  };
}

export function generateNestedElementChild(imageIndex: number): NestedElementChild {
  const originalImageUrl = IMAGES[imageIndex % IMAGES.length]!;

  return {
    id: generateUniqueId(),
    imageUrl: optimizeImageUrl(originalImageUrl, 400),
    title: IMAGE_TITLES[imageIndex % IMAGE_TITLES.length]!,
  };
}

export function generateNestedElement(rowIndex: number): NestedElement {
  const elementsPerRow = 10;
  const elements: NestedElementChild[] = [];

  for (let i = 0; i < elementsPerRow; i++) {
    elements.push(generateNestedElementChild(rowIndex * elementsPerRow + i));
  }

  return {
    id: generateUniqueId(),
    title: SECTION_TITLES[rowIndex % SECTION_TITLES.length]!,
    elements,
  };
}

const MASONRY_HEIGHTS = [180, 220, 260, 200, 240, 280, 190, 230, 250, 210];

export function generateMasonryElement(index: number): MasonryElement {
  const originalImageUrl = IMAGES[index % IMAGES.length]!;

  return {
    id: generateUniqueId(),
    imageUrl: optimizeImageUrl(originalImageUrl, 400),
    title: IMAGE_TITLES[index % IMAGE_TITLES.length]!,
    height: MASONRY_HEIGHTS[index % MASONRY_HEIGHTS.length]!,
  };
}

export function generateContact(index: number): ContactElement {
  const characterName = CHARACTER_NAMES[index % CHARACTER_NAMES.length]!;
  const nameParts = characterName.split(' ');

  const firstName = nameParts[0]!;
  const lastName = nameParts.length > 1 ? nameParts.slice(1).join(' ') : '';

  const areaCode = 100 + (index % 900);
  const exchange = 200 + (index % 800);
  const lineNumber = 1000 + (index % 9000);
  const phoneNumber = `(${areaCode}) ${exchange}-${lineNumber}`;

  return {
    id: generateUniqueId(),
    firstName,
    lastName,
    phoneNumber,
  };
}
