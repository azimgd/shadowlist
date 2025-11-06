import { useState, useRef } from 'react';
import { View, StyleSheet } from 'react-native';
import { Shadowlist, type ShadowListCommands } from 'shadowlist';
import { ChatElement } from './ChatElement';
import { FloatingActionBar } from './FloatingActionBar';
import { MessageInput } from './MessageInput';

const SAMPLE_TEXTS = [
  'The Time Machine stood in the corner. It was made of nickel, ivory, and some transparent crystalline substance. And now I must be explicit, for this that follows is an absolutely unaccountable thing.',
  'As I walked I was watching for every impression that could possibly help to explain the condition of ruinous splendour in which I found the world. The buildings and machines were in decay.',
  'The whole world will be intelligent, educated, and co-operating; things will move faster and faster towards the subjugation of Nature. In the end, wisely and carefully we shall readjust the balance of animal and vegetable life.',
  'So far as I could see, all the world displayed the same exuberant richness as the Thames valley. From every hill I climbed I saw the same abundance of splendid buildings, endlessly varied in material and style.',
  'I cannot describe the strange sensations of time travelling. They are excessively unpleasant. There is a feeling exactly like that one has upon a switchback, of a helpless headlong motion.',
  'The fact is, I was scared of my shadow. The night was very clear. The silence grew more oppressive, and I shivered and trembled. The darkness pressed upon my eyeballs.',
  'I found a groove ripped in it, about midway between the pedestal of the sphinx and the marks of my feet where, on arrival, I had struggled with the overturned machine.',
  'I was in the dark—trapped. So the Morlocks thought. At that I chuckled gleefully. I carefully felt my way along the passage until I came to the light of day.',
  'The moon was setting, and the dying moonlight and the first pallor of dawn were mingled in a ghastly half-light. The bushes were inky black, the ground a sombre grey, the sky colourless and cheerless.',
  'I had got to such a low estimate of her kind that I did not expect any gratitude from her. In that, however, I was wrong. This, I must remind you, was my speculation at the time.',
  'Above me towered the sphinx, upon the bronze pedestal, white, shining, leprous, in the light of the rising moon. It seemed to smile in mockery of my dismay.',
  'Nature never appeals to intelligence until habit and instinct are useless. There is no intelligence where there is no change and no need of change.',
  'I felt hopelessly cut off from my own kind—a strange animal in an unknown world. I must have been a nightmare to them. I had wild visions of marching down the hall and slaying the monster.',
  'The calm of evening was upon the world as I emerged from the great hall, and the scene was lit by the warm glow of the setting sun.',
  'You see, I had always anticipated that the people of the year Eight Hundred and Two Thousand odd would be incredibly in front of us in knowledge, art, everything.',
  'I grieved to think how brief the dream of the human intellect had been. It had committed suicide. It had set itself steadfastly towards comfort and ease.',
  'Even through the veil of my confusion the earth seemed very fair. And so my mind came round to the business of stopping. The peculiar risk lay in the possibility of my finding some substance in the space.',
  'The stars shone brilliantly, and the night was very cold and still. In the morning there was a white frost, and I found a broken cup near the sundial.',
  'I clenched my hands and steadfastly looked into the glaring eyeballs. The machine halted. The dim outlines of a desolate beach grew visible.',
  'It sounds plausible enough tonight, said the Medical Man; but wait until tomorrow. Wait for the common sense of the morning.',
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

function generateRandomText(seed: number): string {
  return SAMPLE_TEXTS[seed % SAMPLE_TEXTS.length]!;
}

function generateOptimizedImageUrl(index: number): string | undefined {
  if (index % 5 !== 0) {
    return undefined;
  }

  const originalImageUrl = ASTRONOMY_IMAGES[index % ASTRONOMY_IMAGES.length]!;
  const cleanUrl = originalImageUrl.replace('https://', '');
  return `https://images.weserv.nl/?url=${cleanUrl}&q=60&w=800`;
}

export const ChatScreen = () => {
  const shadowlistRef = useRef<ShadowListCommands>(null);
  const [data, setData] = useState<{id: string, text: string, isFromMe: boolean, imageUrl?: string}[]>(() =>
    Array.from({ length: 1000 }, (_, index) => ({
      id: generateUniqueId(),
      text: generateRandomText(index),
      isFromMe: index % 3 !== 0,
      imageUrl: generateOptimizedImageUrl(index),
    }))
  );

  const handlePrepend = () => {
    const currentLength = data.length;
    const newElements = Array.from({ length: 10 }, (_, index) => ({
      id: generateUniqueId(),
      text: generateRandomText(currentLength + index),
      isFromMe: (currentLength + index) % 3 !== 0,
      imageUrl: generateOptimizedImageUrl(currentLength + index),
    }));
    setData((prev) => [...newElements, ...prev]);
  };

  const handleAppend = () => {
    const currentLength = data.length;
    const newElements = Array.from({ length: 10 }, (_, index) => ({
      id: generateUniqueId(),
      text: generateRandomText(currentLength + index),
      isFromMe: (currentLength + index) % 3 !== 0,
      imageUrl: generateOptimizedImageUrl(currentLength + index),
    }));
    setData((prev) => [...prev, ...newElements]);
  };

  const handleSendMessage = (message: string) => {
    const newMessage = {
      id: generateUniqueId(),
      text: message,
      isFromMe: true,
      imageUrl: undefined,
    };
    setData((prev) => [...prev, newMessage]);
  };

  return (
    <View style={styles.container}>
      <Shadowlist
        data={data}
        ref={shadowlistRef}
        style={styles.list}
        renderItem={({ item: element, index }) => (
          <ChatElement
            id={element.id}
            index={index}
            text={element.text}
            isFromMe={element.isFromMe}
            imageUrl={element.imageUrl}
          />
        )}
      />
      <FloatingActionBar onPrepend={handlePrepend} onAppend={handleAppend} />
      <MessageInput onSend={handleSendMessage} />
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
