import { useState, useRef } from 'react';
import { faker } from '@faker-js/faker';

const TIMEOUT = 1000;

const initialState = (length: number) =>
  Array.from({ length }, (_, position) => ({
    __shadowlist_template_id:
      position % 2 === 0
        ? 'ListTemplateComponentUniqueIdYarrow'
        : 'ListTemplateComponentUniqueIdRobin',
    id: faker.database.mongodbObjectId(),
    title: faker.person.fullName(),
    text: faker.lorem.paragraph(),
    subtitle: faker.person.jobTitle(),
    color: faker.color.rgb(),
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

  const update = (index: number) => {
    setData((state) => {
      const newState = [...state];
      newState.splice(index, 1, {
        __shadowlist_template_id: 'ListTemplateComponentUniqueIdRobin',
        id: faker.database.mongodbObjectId(),
        title: faker.person.fullName(),
        text: faker.lorem.paragraph(),
        subtitle: faker.person.jobTitle(),
        color: faker.color.rgb(),
        image: faker.image.avatarGitHub(),
        position: index.toString(),
      });
      return newState;
    });
  };

  return { data: data, loadPrepend, loadAppend, update };
};

export default useData;
