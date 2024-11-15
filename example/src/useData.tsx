import { useState, useRef } from 'react';
import { faker } from '@faker-js/faker';

const initialState = (length: number) =>
  Array.from({ length }, () => ({
    id: faker.database.mongodbObjectId(),
    text: faker.lorem.paragraph(),
  }));

const useData = ({
  length,
  inverted,
}: {
  length: number;
  inverted: boolean;
}) => {
  const loading = useRef(false);
  const [data, setData] = useState(initialState(length));

  const load = () => {
    if (loading.current) return;

    setTimeout(() => {
      setData((state) => {
        loading.current = false;
        return !inverted
          ? [...state, ...initialState(length)]
          : [...initialState(length), ...state];
      });
    }, 1000);
    loading.current = true;
  };

  return { data: data, load };
};

export default useData;
