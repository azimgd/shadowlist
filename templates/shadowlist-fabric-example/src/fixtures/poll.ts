import type { PollData } from 'shadowlist-utils/native';

export const destinationPoll: PollData = {
  question: 'Where should we travel next?',
  options: [
    { id: 'lisbon', label: 'Lisbon', votes: 12 },
    { id: 'kyoto', label: 'Kyoto', votes: 9 },
    { id: 'reykjavik', label: 'Reykjavík', votes: 7 },
    { id: 'oaxaca', label: 'Oaxaca', votes: 5 },
  ],
};
