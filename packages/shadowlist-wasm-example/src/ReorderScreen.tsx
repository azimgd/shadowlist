import { useState, type CSSProperties } from 'react';
import { Reorder, ListHeader, colors } from 'shadowlist-utils/web';
import { generateContact, type ContactItem } from 'shadowlist-utils';

export const ReorderScreen = () => {
  const [data, setData] = useState<ContactItem[]>(() =>
    Array.from({ length: 80 }, (_, index) => generateContact(index))
  );

  return (
    <div style={styles.container}>
      <Reorder.List
        data={data}
        style={styles.list}
        onReorder={({ data: reordered }) => setData(reordered)}
        renderElement={({ element }) => <Reorder.Row element={element} />}
        ListHeaderComponent={
          <ListHeader
            title="Reorder"
            subtitle="Press and hold a row, then drag to reorder"
          />
        }
      />
    </div>
  );
};

const styles: Record<string, CSSProperties> = {
  container: {
    position: 'relative',
    display: 'flex',
    flexDirection: 'column',
    flex: 1,
    minHeight: 0,
    background: colors.background,
  },
  list: {
    flex: 1,
    minHeight: 0,
    background: colors.background,
  },
};
