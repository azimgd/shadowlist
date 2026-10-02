import { useState } from 'react';
import { ScrollView, StyleSheet, Text, View } from 'react-native';
import {
  Assistant,
  Avatar,
  Chip,
  ChipRow,
  ChoiceChip,
  CopyIcon,
  DashedButton,
  DocIcon,
  EmptyState,
  Form,
  FolderIcon,
  Grouped,
  IconButton,
  IndexBadge,
  ItemSeparator,
  ListFooter,
  ListHeader,
  PillButton,
  PlusIcon,
  ProgressBar,
  ProgressRing,
  RetryIcon,
  SearchField,
  SectionHeader,
  Segmented,
  Spinner,
  TemplateList,
  TextField,
  createStyles,
  useTheme,
  type TemplateMap,
} from 'shadowlist-utils/native';
import { TemplatesSection } from './TemplatesSection';
import { TemplatesShelfCount } from './TemplatesShelfCount';

type Period = 'day' | 'week' | 'month';
type Order = 'asc' | 'desc';

const PERIODS = [
  { id: 'day', label: 'Day' },
  { id: 'week', label: 'Week' },
  { id: 'month', label: 'Month' },
] as const;

const ORDERS = [
  { id: 'asc', label: 'A→Z' },
  { id: 'desc', label: 'Z→A' },
] as const;

const CHOICES = ['Planned', 'Booked', 'Done'];

type ShelfRow =
  | { id: string; kind: 'tag'; label: string }
  | { id: string; kind: 'count'; count: number };

const SHELF: ShelfRow[] = [
  { id: 'tag-1', kind: 'tag', label: 'Lisbon' },
  { id: 'count-1', kind: 'count', count: 12 },
  { id: 'tag-2', kind: 'tag', label: 'Kyoto' },
  { id: 'count-2', kind: 'count', count: 7 },
  { id: 'tag-3', kind: 'tag', label: 'Reykjavík' },
  { id: 'tag-4', kind: 'tag', label: 'Oaxaca' },
];

const noop = () => {};

const SHELF_TEMPLATES: TemplateMap<ShelfRow> = {
  tag: ({ label }) => <Chip label={label} onPress={noop} />,
  count: ({ count }) => <TemplatesShelfCount count={count} />,
};

export const TemplatesScreen = () => {
  const styles = useStyles();
  const { colors } = useTheme();
  const [query, setQuery] = useState('');
  const [code, setCode] = useState('');
  const [notes, setNotes] = useState('');
  const [period, setPeriod] = useState<Period>('week');
  const [order, setOrder] = useState<Order>('asc');
  const [choice, setChoice] = useState(CHOICES[0]);
  const [name, setName] = useState('Boarding pass');
  const [price, setPrice] = useState('42.50');
  const [pressed, setPressed] = useState('Nothing yet');

  return (
    <ScrollView
      style={styles.page}
      contentContainerStyle={styles.content}
      contentInsetAdjustmentBehavior="automatic"
      keyboardShouldPersistTaps="handled"
    >
      <TemplatesSection title="Buttons">
        <View style={styles.wrap}>
          <PillButton label="Save" onPress={noop} />
          <PillButton label="Undo" variant="tinted" onPress={noop} />
          <PillButton label="Cancel" variant="plain" onPress={noop} />
          <PillButton label="Delete" variant="destructive" onPress={noop} />
          <PillButton label="Disabled" disabled onPress={noop} />
          <IconButton
            icon={<PlusIcon size={19} color={colors.accent} />}
            accessibilityLabel="Add"
            onPress={noop}
          />
        </View>
        <DashedButton label="Add sort" onPress={noop} />
      </TemplatesSection>

      <TemplatesSection title="Chips">
        <ChipRow>
          <Chip label="Fill down" onPress={noop} />
          <Chip label="Clear" variant="plain" onPress={noop} />
        </ChipRow>
        <ChipRow>
          {CHOICES.map((label, index) => (
            <ChoiceChip
              key={label}
              label={label}
              tint={
                [colors.accentSoft, colors.redSoft, colors.fill][index] ??
                colors.fill
              }
              selected={choice === label}
              onPress={() => setChoice(label)}
            />
          ))}
        </ChipRow>
      </TemplatesSection>

      <TemplatesSection title="Fields">
        <SearchField value={query} onChange={setQuery} />
        <TextField value={code} onChange={setCode} placeholder="Gate code" />
        <TextField
          value={notes}
          onChange={setNotes}
          placeholder="Paste rows here"
          multiline
        />
      </TemplatesSection>

      <TemplatesSection title="Segmented">
        <Segmented
          options={PERIODS}
          value={period}
          onChange={setPeriod}
          accessibilityLabel="Period"
        />
        <Segmented
          options={ORDERS}
          value={order}
          onChange={setOrder}
          size="compact"
          accessibilityLabel="Order"
        />
      </TemplatesSection>

      <TemplatesSection title="Form">
        <Form.Card>
          <Form.Row label="Name">
            <Form.Input
              value={name}
              onChange={setName}
              accessibilityLabel="Name"
            />
          </Form.Row>
          <Form.Row label="Price">
            <Form.Input
              value={price}
              onChange={setPrice}
              numeric
              accessibilityLabel="Price"
            />
          </Form.Row>
          <Form.Row label="With tax">
            <Form.Value text={formatTotal(price)} />
          </Form.Row>
          <Form.Row label="Period" stacked>
            <Segmented
              options={PERIODS}
              value={period}
              onChange={setPeriod}
              style={styles.grow}
            />
          </Form.Row>
        </Form.Card>
        <Form.Button label="Remove item" destructive onPress={noop} />
      </TemplatesSection>

      <View style={styles.grouped}>
        <Grouped.Section title="Grouped">
          <Grouped.Row
            title="Trip files"
            subtitle="12 documents"
            leading={
              <View style={[styles.tile, { backgroundColor: colors.accent }]}>
                <FolderIcon size={18} color={colors.onAccent} />
              </View>
            }
            onPress={() => setPressed('Trip files')}
            onLongPress={() => setPressed('Trip files, held')}
            longPressLabel="Show details"
          />
          <Grouped.Row
            title="Currency"
            trailing={<Text style={styles.value}>EUR</Text>}
            onPress={() => setPressed('Currency')}
            separated
          />
          <Grouped.Row
            title="Export"
            subtitle="A long description that wraps onto a second line when it needs to"
            subtitleLines={2}
            chevron={false}
            onPress={() => setPressed('Export')}
            separated
          />
          <Grouped.Row
            title="Sync"
            subtitle="Unavailable offline"
            disabled
            onPress={noop}
            separated
          />
          <Grouped.Separator inset={0} />
          <Grouped.Row title={`Last press: ${pressed}`} chevron={false} />
        </Grouped.Section>
      </View>

      <TemplatesSection title="Progress" card>
        <ProgressBar fraction={0.25} accessibilityLabel="25% saved" />
        <ProgressBar
          fraction={0.62}
          color={colors.green}
          accessibilityLabel="62% saved"
        />
        <ProgressBar
          fraction={0.9}
          color={colors.orange}
          accessibilityLabel="90% spent"
        />
        <ProgressBar
          fraction={1.2}
          color={colors.red}
          height={10}
          accessibilityLabel="Over budget"
        />
        <View style={styles.wrap}>
          <ProgressRing fraction={0.25} />
          <ProgressRing fraction={0.6} />
          <ProgressRing fraction={0.9} size={40} strokeWidth={5} />
        </View>
      </TemplatesSection>

      <TemplatesSection title="Badges">
        <View style={styles.wrap}>
          <IndexBadge index={1} />
          <IndexBadge index={2} />
          <IndexBadge index={3} />
        </View>
      </TemplatesSection>

      <TemplatesSection title="Empty state" card>
        <EmptyState
          icon={<DocIcon size={28} color={colors.tertiaryLabel} />}
          title="No files yet"
          message="Add one below."
        />
      </TemplatesSection>

      <TemplatesSection title="Primitives" card>
        <View style={styles.wrap}>
          <Avatar name="Ada Lovelace" />
          <Avatar name="Grace Hopper" size={32} />
          <Spinner />
        </View>
        <ListHeader title="List header" subtitle="With a subtitle" />
        <SectionHeader title="Section header" count={3} />
        <ItemSeparator />
        <ListFooter text="List footer" />
      </TemplatesSection>

      <TemplatesSection title="Assistant">
        <View style={styles.wrap}>
          <Assistant.ActionButton label="Copy" onPress={noop}>
            <CopyIcon size={18} color={colors.secondaryLabel} />
          </Assistant.ActionButton>
          <Assistant.ActionButton label="Retry" onPress={noop} showLabel>
            <RetryIcon size={18} color={colors.label} />
          </Assistant.ActionButton>
        </View>
      </TemplatesSection>

      <TemplatesSection title="Template list">
        <TemplateList
          horizontal
          data={SHELF}
          templates={SHELF_TEMPLATES}
          style={styles.shelf}
          elementStyle={styles.shelfElement}
          initialElementsSize={96}
          accessibilityLabel="Destinations"
        />
      </TemplatesSection>
    </ScrollView>
  );
};

function formatTotal(price: string): string {
  const value = Number.parseFloat(price);
  return Number.isFinite(value) ? (value * 1.2).toFixed(2) : '';
}

const useStyles = createStyles(({ colors, typography, spacing }) =>
  StyleSheet.create({
    page: {
      flex: 1,
      backgroundColor: colors.groupedBackground,
    },
    content: {
      paddingBottom: spacing.xxl * 2,
    },
    wrap: {
      flexDirection: 'row',
      flexWrap: 'wrap',
      alignItems: 'center',
      gap: spacing.sm,
    },
    grow: {
      flex: 1,
    },
    grouped: {
      marginTop: spacing.xxl,
    },
    tile: {
      width: 30,
      height: 30,
      borderRadius: 7,
      alignItems: 'center',
      justifyContent: 'center',
    },
    value: {
      ...typography.body,
      color: colors.secondaryLabel,
    },
    shelf: {
      height: 34,
    },
    shelfElement: {
      marginRight: spacing.sm,
    },
  })
);
