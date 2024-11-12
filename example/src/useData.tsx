import { useState, useRef } from 'react';
import { faker } from '@faker-js/faker';

const initialState = () =>
  Array.from({ length: 50 }, () => ({
    id: faker.database.mongodbObjectId(),
    text: faker.lorem.paragraph(),
  }));

const useData = () => {
  const loading = useRef(false);
  const [data, setData] = useState(initialState());

  const load = () => {
    if (loading.current) return;

    setTimeout(() => {
      setData((state) => {
        loading.current = false;
        return state.concat(initialState());
      });
    }, 1000);
    loading.current = true;
  };

  return { data: data, load };
};

export default useData;
