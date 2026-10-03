import type { NativeStackScreenProps } from '@react-navigation/native-stack';
import type { RootStackParamList } from '../routes';
import { ContactDetail } from './ContactDetail';

type Props = NativeStackScreenProps<RootStackParamList, 'ContactDetail'>;

export const ContactDetailScreen = ({ route }: Props) => (
  <ContactDetail contact={route.params.contact} />
);
