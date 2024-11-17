import { useState, useRef } from 'react';
import { faker } from '@faker-js/faker';

const initialState = (length: number) =>
  Array.from({ length }, () => ({
    id: faker.database.mongodbObjectId(),
    text: faker.lorem.paragraph(),
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
    }, 3000);
    loading.current = true;
  };

  const loadAppend = () => {
    if (loading.current) return;

    setTimeout(() => {
      setData((state) => {
        loading.current = false;
        return [...state, ...initialState(length)];
      });
    }, 3000);
    loading.current = true;
  };

  return { data: data, loadPrepend, loadAppend };
};

export default useData;
