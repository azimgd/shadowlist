import { useState, useRef } from 'react';
import { View, StyleSheet } from 'react-native';
import { Shadowlist, type ShadowListCommands } from 'shadowlist';
import { FloatingActionBar } from './FloatingActionBar';
import { FeedElement, type FeedElement as FeedElementType } from './FeedElement';

const SAMPLE_TEXTS = [
  'The Time Traveller (for so it will be convenient to speak of him) was expounding a recondite matter to us. His grey eyes shone and twinkled, and his usually pale face was flushed and animated.',
  'There are really four dimensions, three which we call the three planes of Space, and a fourth, Time. There is, however, a tendency to draw an unreal distinction between the former three dimensions and the latter.',
  'Scientific people know very well that Time is only a kind of Space. Here is a popular scientific diagram, a weather record. This line I trace with my finger shows the movement of the barometer.',
  'Clearly, the Time Traveller proceeded, any real body must have extension in four directions: it must have Length, Breadth, Thickness, and Duration.',
  'You must follow me carefully. I shall have to controvert one or two ideas that are almost universally accepted. The geometry, for instance, they taught you at school is founded on a misconception.',
  'You know of course that a mathematical line, a line of thickness nil, has no real existence. They taught you that? Neither has a mathematical plane. These things are mere abstractions.',
  'Can a cube that does not last for any time at all, have a real existence? Clearly any real body must have extension in four directions.',
  'There is no difference between Time and any of the three dimensions of Space except that our consciousness moves along it.',
  'Some philosophical people have been asking why three dimensions particularly—why not another direction at right angles to the other three?',
  'I think that at that time none of us quite believed in the Time Machine. The fact is, the Time Traveller was one of those men who are too clever to be believed.',
  'He took one of the small octagonal tables that were scattered about the room, and set it in front of the fire, with two legs on the hearthrug.',
  'I drew a breath, set my teeth, gripped the starting lever with both hands, and went off with a thud. The laboratory got hazy and went dark.',
  'The laboratory was exactly as I had left it, save for that one thing. The Time Machine was gone!',
  'I seemed to reel; I felt a nightmare sensation of falling; and, looking round, I saw the laboratory exactly as before.',
  'Presently I noted that the sun belt swayed up and down, from solstice to solstice, in a minute or less, and that consequently my pace was over a year a minute.',
  'The peculiar risk lay in the possibility of my finding some substance in the space which I, or the machine, occupied. So long as I travelled at a high velocity through time, this scarcely mattered.',
  'I saw trees growing and changing like puffs of vapour, now brown, now green; they grew, spread, shivered, and passed away. I saw huge buildings rise up faint and fair, and pass like dreams.',
  'I saw great and splendid architecture rising about me, more massive than any buildings of our own time, and yet, as it seemed, built of glimmer and mist.',
  'Looking at these stars suddenly dwarfed my own troubles and all the gravities of terrestrial life. I thought of their unfathomable distance, and the slow inevitable drift of their movements out of the unknown past into the unknown future.',
  'We are always getting away from the present moment. Our mental existences, which are immaterial and have no dimensions, are passing along the Time-Dimension with a uniform velocity from the cradle to the grave.',
];

const ASTRONOMY_IMAGES = [
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

function generateUniqueId(): string {
  return `${Date.now()}-${Math.random().toString(36).substr(2, 9)}`;
}

function generateFeedElement(index: number): FeedElementType {
  const userNum = (index % 50) + 1;
  const originalImageUrl = ASTRONOMY_IMAGES[index % ASTRONOMY_IMAGES.length]!;

  const cleanUrl = originalImageUrl.replace('https://', '');
  const optimizedImageUrl = `https://images.weserv.nl/?url=${cleanUrl}&q=60&w=800`;

  return {
    id: generateUniqueId(),
    username: `User ${userNum}`,
    handle: `@user${userNum}`,
    text: SAMPLE_TEXTS[index % SAMPLE_TEXTS.length]!,
    imageUrl: optimizedImageUrl,
    timestamp: `${Math.floor(Math.random() * 24)}h`,
  };
}

export const FeedScreen = () => {
  const shadowlistRef = useRef<ShadowListCommands>(null);
  const [data, setData] = useState<FeedElementType[]>(() =>
    Array.from({ length: 100 }, (_, index) => generateFeedElement(index))
  );

  const handlePrepend = () => {
    const currentLength = data.length;
    const newElements = Array.from({ length: 10 }, (_, index) =>
      generateFeedElement(currentLength + index)
    );
    setData((prev) => [...newElements, ...prev]);
  };

  const handleAppend = () => {
    const currentLength = data.length;
    const newElements = Array.from({ length: 10 }, (_, index) =>
      generateFeedElement(currentLength + index)
    );
    setData((prev) => [...prev, ...newElements]);
  };

  return (
    <View style={styles.container}>
      <Shadowlist
        data={data}
        ref={shadowlistRef}
        style={styles.list}
        renderItem={({ item: element, index }) => (
          <FeedElement element={element} index={index} />
        )}
      />
      <FloatingActionBar onPrepend={handlePrepend} onAppend={handleAppend} />
    </View>
  );
};

const styles = StyleSheet.create({
  container: {
    flex: 1,
    backgroundColor: '#000000',
  },
  list: {
    flex: 1,
    backgroundColor: '#000000',
  },
});
