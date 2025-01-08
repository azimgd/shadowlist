import { useState, useRef } from 'react';
import { faker } from '@faker-js/faker';

const TIMEOUT = 3000;

const initialState = (length: number) =>
  Array.from({ length }, (_, position) => ({
    id: faker.database.mongodbObjectId(),
    text: faker.lorem.paragraph(),
    image: faker.image.avatarGitHub(),
    position: position.toString(),
  }));

const useData = ({ length }: { length: number; inverted?: boolean }) => {
  const loading = useRef(false);
  const [data, setData] = useState(initialState(length));

  const loadPrepend = () => {
    if (loading.current) return;

    setTimeout(() => {
      setData((state) => {
        loading.current = false;
        return [...initialState(length), ...state];
      });
    }, TIMEOUT);
    loading.current = true;
  };

  const loadAppend = () => {
    if (loading.current) return;

    setTimeout(() => {
      setData((state) => {
        loading.current = false;
        return [...state, ...initialState(length)];
      });
    }, TIMEOUT);
    loading.current = true;
  };

  return { data: data, loadPrepend, loadAppend };
};

export default useData;
