import {
  emptyTurn,
  type AssistantAttachment,
  type AssistantMessage,
  type AssistantPrompt,
  type AssistantReply,
  type AssistantSuggestion,
  type AssistantTurnWriter,
} from 'shadowlist-utils/native';
import { generateUniqueId } from './common';

/*
 * There is no model behind the Assistant demo: a prompt picks a script by keyword and
 * playScript feeds it token by token into a turn writer, exercising the list the way a real
 * stream does.
 */

export interface AssistantScript {
  thinking?: string;
  toolCalls?: {
    name: string;
    input: string;
    output: string;
    fails?: boolean;
  }[];
  content: string;
  sources?: { title: string; domain: string; path: string }[];
  followUps?: string[];
  failAt?: number;
  error?: string;
}

export const ASSISTANT_MODELS = ['Cruise', 'Jet', 'Long-haul'];

export const ASSISTANT_SUGGESTIONS: AssistantSuggestion[] = [
  {
    title: 'Write an itinerary',
    prompt: 'Write a three-day itinerary for Lisbon in May',
  },
  {
    title: 'Compare fares',
    prompt: 'Compare fares to Tokyo across airlines in a table',
  },
  {
    title: 'Search flights',
    prompt: 'Search Skyfy for flights to Reykjavik',
  },
  {
    title: 'Simulate a failure',
    prompt: 'Simulate a dropped connection error',
  },
];

// The fallback prompt a reply to attachments alone is generated for.
export const ATTACHMENTS_ONLY_PROMPT = 'Describe the attached tickets';

const ATTACHMENT_COLORS = [
  '#FF6B6B',
  '#4ECDC4',
  '#45B7D1',
  '#FFA07A',
  '#98D8C8',
  '#F7DC6F',
  '#BB8FCE',
  '#85C1E2',
  '#F8B195',
  '#C06C84',
];

const ATTACHMENT_SEEDS: Pick<
  AssistantAttachment,
  'kind' | 'name' | 'detail'
>[] = [
  { kind: 'image', name: 'boarding-pass.png', detail: '1.4 MB' },
  { kind: 'file', name: 'lisbon-itinerary.pdf', detail: '312 KB' },
  { kind: 'file', name: 'hotel-booking.pdf', detail: '64 KB' },
];

// Stands in for a file picker: cycles the seed set, with a fresh id so repeated picks stay distinct.
export function buildAttachment(index: number): AssistantAttachment {
  return {
    id: generateUniqueId(),
    color: ATTACHMENT_COLORS[(index * 3) % ATTACHMENT_COLORS.length]!,
    ...ATTACHMENT_SEEDS[index % ATTACHMENT_SEEDS.length]!,
  };
}

const CODE_SCRIPT: AssistantScript = {
  thinking:
    'They want a three-day Lisbon plan in May. Packing five sights into each day is the usual mistake: Lisbon is hills and trams, and a crowded list turns into an afternoon at tram stops. Group by neighbourhood, save viewpoints for sunset, and keep the last day light.',
  content: [
    "Here's a three-day Lisbon itinerary built around **one neighbourhood per day** instead of one sight per hour. It is simple, but it is the piece that decides whether the trip feels relaxed or turns into a string of tram queues on steep hills.",
    '',
    '```yaml',
    'trip: Lisbon, 3 days in May',
    'arrive: LIS, day 1 at 09:10',
    'day_1:',
    '  area: Alfama and Baixa',
    '  morning: Tram 28 to Graça, walk down',
    '  lunch: Petiscos near Largo do Chafariz',
    '  afternoon: Castelo de São Jorge',
    '  sunset: Miradouro de Santa Luzia',
    'day_2:',
    '  area: Belém',
    '  morning: Jerónimos Monastery at 09:30',
    '  lunch: Custard tarts, go early',
    '  afternoon: MAAT and the riverside walk',
    '  evening: Dinner in Cais do Sodré',
    'day_3:',
    '  area: Príncipe Real and Chiado',
    '  morning: Garden market and coffee',
    '  afternoon: Pack, late checkout',
    '  depart: LIS at 18:45, leave by 16:15',
    'weather: 19-24°C, low chance of rain',
    '```',
    '',
    'The first thing to notice is that each day stays in one part of the city. Lisbon looks compact on a map, but the hills and the tram timetable turn a two-kilometre hop into forty minutes. Keeping the morning, lunch and afternoon close together means you spend the day in the city, not crossing it.',
    '',
    'Sunsets are what the plan is built around. Lisbon faces west over the river, and the viewpoints fill up about half an hour before the sun goes down. Day one ends at Santa Luzia for exactly that reason, and the other evenings are left open for wherever the light takes you.',
    '',
    'Belém gets a whole day because everything there opens late and queues early. Reach the monastery just before it opens, eat the custard tarts before the tour buses arrive, and let the long riverside walk to MAAT fill the afternoon at an easy pace. It is the one day worth an early alarm.',
    '',
    'Day three is deliberately light. Your flight leaves at 18:45, and Skyfy suggests leaving the city by 16:15 to clear security comfortably. A late checkout, one garden market and a slow coffee is all that day needs, and it keeps the trip from ending in a sprint to the gate.',
    '',
    'Weather is the last detail worth checking. May in Lisbon is warm but breezy along the water, so pack a light layer for the evenings. I will keep watching the forecast for your dates and flag anything that changes before you fly, including rain on the day you planned for the viewpoints.',
  ].join('\n'),
  followUps: ['Add restaurant picks', 'Make it a weekend trip'],
};

const TABLE_SCRIPT: AssistantScript = {
  thinking:
    'Three kinds of ticket cover the route: nonstop, one stop through Helsinki, and a budget fare via Dubai. A table first, then the trade-off that decides each.',
  content: [
    '## Fares to Tokyo',
    '',
    'Every long-haul trip ends up choosing between the same three kinds of ticket. They differ less in the price on the screen than in how many hours you give up and what happens when something goes wrong along the way.',
    '',
    '| Option | Stops | Travel time | Best for |',
    '| --- | --- | --- | --- |',
    '| Nonstop | None | 13h 50m | Short trips |',
    '| One-stop | Helsinki | 17h 20m | Saving a little |',
    '| Budget | Dubai | 21h 05m | Flexible dates, long stays |',
    '',
    '**Nonstop** gets you there in one go and costs the most. It is the easiest ticket to reason about, because there is no connection to miss and your bag never changes planes, but the price climbs quickly in peak season, and the best departure times sell out weeks before the rest.',
    '',
    "**One-stop** through Helsinki saves a meaningful amount and adds only a few hours. It is the best value for most travellers, since the connection is short and the airport is calm, but a late inbound flight puts that tight layover at risk, and a missed connection can cost you the next day's plans.",
    '',
    '**Budget** via Dubai is the cheapest by a wide margin and the longest by far. The fare looks great until you add a checked bag, a seat selection and a meal, and the overnight layover means arriving tired. That is what makes it work for *long stays*: an extra day of recovery barely matters on a three-week trip.',
    '',
    "Short trips are the hardest case for all three, because every lost hour comes straight out of your time on the ground. The budget fare eats a full day, the one-stop fare risks a missed connection, and only the nonstop flight keeps the trip's first evening safe.",
    '',
    '---',
    '',
    'Pick by trip length first, then by price. Short trips favour nonstop, a normal week is fine with one stop, and anything longer than two weeks can absorb the savings and the extra hours of the budget fare.',
  ].join('\n'),
  followUps: ['Which one has Wi-Fi?', 'Show seat maps'],
};

const RESEARCH_SCRIPT: AssistantScript = {
  thinking:
    'Search Skyfy for flights to Reykjavik before answering, then check the weather at Keflavik for the dates.',
  toolCalls: [
    {
      name: 'search_flights',
      input: '{ "to": "KEF", "month": "2027-03" }',
      output: '3 results · nonstop 5h 40m, one-stop via Oslo, red-eye',
    },
    /*
     * Deliberately fails, so the tool error UI is reachable from a prompt. The answer is
     * written from the search result above it either way, which is what a real assistant
     * does when one fetch of several times out.
     */
    {
      name: 'get_weather',
      input: '{ "airport": "KEF", "days": 7 }',
      output: '504 · upstream timeout',
      fails: true,
    },
  ],
  content: [
    'Skyfy found **three** good ways to reach Reykjavik in March, and the right one depends mostly on when you want to land. All three arrive at Keflavik, about forty-five minutes from the city by the airport bus, which meets every arriving flight.',
    '',
    'The nonstop is the obvious pick: five hours forty minutes, a mid-morning departure and an early evening arrival. It lands in daylight, so the drive across the lava fields is part of the trip rather than a dark blur, and it leaves time for a first dinner in town without rushing. The return leg leaves Reykjavik mid-afternoon, which suits a last slow morning in the city.',
    '',
    'The one-stop through Oslo is cheaper and adds about three hours. The layover is comfortable rather than tight, and Oslo airport is easy to change planes in. It is worth considering if your dates are flexible, since the savings grow the further you book ahead of the trip.',
    '',
    'The red-eye is the **cheapest** option and the one to be careful with. It lands before six in the morning, hours before any hotel will check you in, and March mornings in Iceland are cold and dark. If you take it, book the first night anyway so a room is ready when you arrive. An arrivals lounge helps a little, but a bed helps far more.',
    '',
    'I could not reach the weather service for your dates, so I have not included a forecast. March is typically changeable, with short bright spells between wind and snow, so plan any driving for the middle of the day and check road conditions each morning before you set off.',
    '',
    'For most travellers that leaves a simple rule: take the nonstop if daylight matters, the Oslo connection if price matters, and the red-eye only with a room waiting. See [Keflavik arrivals](https://skyfy.example.com/airports/kef) for transfers, luggage storage and the notes on late arrivals.',
  ].join('\n'),
  sources: [
    {
      title: 'Flights to Reykjavik',
      domain: 'skyfy.example.com',
      path: '/flights/kef',
    },
    {
      title: 'Keflavik airport guide',
      domain: 'skyfy.example.com',
      path: '/airports/kef',
    },
    {
      title: 'Iceland in March',
      domain: 'guides.example.com',
      path: '/iceland-march',
    },
  ],
  followUps: ['Show the red-eye option', 'What about the weather?'],
};

const FAILURE_SCRIPT: AssistantScript = {
  content: [
    'Starting a full plan for a cancelled flight so there is plenty on screen when the connection drops. A cancellation can happen at any point in a trip, and the traveller needs something clear to act on rather than a wall of options and no next step.',
    '',
    'The first rule is to keep your booking reference in reach. Rebooking desks, apps and phone lines all start from it, and having it ready before you reach the front of a queue can save the last seat on the next flight.',
    '',
    'The second rule is to rebook before you queue. Skyfy shows alternative flights the moment a cancellation posts, and the seats on them go fast. Rebooking in the app first and confirming at the desk later is almost always quicker.',
    '',
    'The third rule is to ask what you are owed. Depending on the route and the reason, a cancellation can include meals, a hotel for the night and compensation, and it is far easier to claim while you are still at the airport.',
    '',
    'Everything past this point would have covered how to protect a connecting flight on a separate ticket, and how to tell a delay from a cancellation so you know when to start looking for another route.',
    '',
    'It would also have covered keeping every receipt from the disruption, so a claim for meals, transport or a hotel room is never rejected for lack of proof.',
  ].join('\n'),
  failAt: 0.45,
  error: 'The connection was interrupted.',
};

// What the failure prompt plays on a retry or regeneration: the connection holds this time.
const RECOVERED_SCRIPT: AssistantScript = {
  content: [
    'Reconnected -- here is the whole answer this time.',
    '',
    'When a flight is cancelled, Skyfy keeps your original booking visible and marks it clearly, so you never mistake a cancelled seat for a confirmed one. That state is deliberately obvious: a banner on the trip card and a rebook button, rather than a quiet change buried in your email.',
    '',
    'Rebooking moves you onto a new flight within the same trip instead of creating a new one. The cancelled leg is replaced in place, so once rebooking succeeds your itinerary still reads as one journey, with no orphaned half-booking left above it to confuse your crew.',
    '',
    'Changing your plans is different on purpose. It keeps the original option and adds a new one, and the trip view lets you compare them side by side. Rebooking fixes a disruption; changing plans is a choice you make.',
    '',
    'Behind both, your crew sees only what changed. Their copy of the itinerary updates once when the new flight is confirmed, and everyone travelling with you gets a single notification, so a disruption costs them no more than any other update.',
    '',
    'For a smoother day, turn on flight alerts before you travel, keep a charger in your carry-on, and save the airline phone number in your contacts so you can call while you queue. The traveller who acts first usually gets the best seat.',
    '',
    'Hotels and transfers follow the same rule. A booking that depended on the cancelled flight is flagged for review rather than left unchanged, and Skyfy offers to move it to match the new arrival time, so a stale reservation from the original plan never leaves you stranded at night.',
  ].join('\n'),
  followUps: ['Simulate another failure'],
};

const GENERAL_SCRIPTS: AssistantScript[] = [
  {
    content: [
      'Short answer: book **six to eight weeks out** for most short-haul trips. The rest of this is why that one window matters so much.',
      '',
      'Airlines release each flight in fare classes, from cheapest to most expensive, and sell them in order. Early on, the cheap classes are open but demand is uncertain; very close to departure, the cheap classes are gone and only business travellers are left buying, so prices climb quickly. That is why the same seat can cost twice as much a week before departure.',
      '',
      'The sweet spot sits in between. The cheapest seats have not sold out yet, but airlines have enough booking data to start discounting routes that are filling slowly. Skyfy watches that window for you and highlights the fare when it dips.',
      '',
      'The day you book matters less than people think. What matters is the day you fly: midweek flights are usually cheaper than Fridays and Sundays, because that is when fewer people want to travel. Tuesday and Wednesday departures are often the cheapest of the week.',
      '',
      'Fare alerts sit on top of that. Set one when you start planning and Skyfy checks the route every few hours, which catches short sales that nobody would spot by searching once a day.',
      '',
      'Once you book, keep the alert running for a day or two. Some airlines let you rebook at a lower fare without a fee, and the difference comes back as credit for your next trip rather than disappearing. Keep the confirmation email handy.',
    ].join('\n'),
    followUps: ['Why fly midweek?', 'Set a fare alert'],
  },
  {
    thinking:
      'General question. Keep it concrete and walk through the two habits frequent flyers notice first.',
    content: [
      'Two things matter most for a long flight that feels short, and neither is the seat you pay for.',
      '',
      'The first is **timing your sleep**. Your body reads light, food and screens as clues about the time of day, and a cabin full of all three at random hours confuses it. Setting your watch to the destination as you board gives you a single rhythm to follow.',
      '',
      'The second is **moving only when the cabin is calm**. While meals are served the aisle is blocked, and standing up just means waiting. The moment the carts are stowed, a short walk and a stretch do more for your legs than any pillow.',
      '',
      'That second habit is where most long flights go wrong. If you wait for a quiet moment that never comes, you stay folded in your seat for hours, and every hour makes the ache in your back and ankles harder to shake after landing.',
      '',
      'The fix is to treat the end of each service as a signal. Stand up right away, walk to the galley and back, drink a full glass of water, and set a reminder for two hours later so the habit keeps going without you thinking about it.',
      '',
      'Landing in daylight should reset the rest on its own, so a nap on arrival is a shortcut rather than the only way back. Together those habits make long flights feel calm: you arrive tired in the normal way, and nothing more.',
    ].join('\n'),
    followUps: ['How do I beat jet lag?'],
  },
  {
    content: [
      "Happy to help. I'm the Skyfy Assistant, and each kind of question shows off a different part of how I plan trips.",
      '',
      'Ask me to **write** an itinerary to get a day-by-day plan in a block with a label and a copy button. The plan scrolls sideways instead of wrapping, so long lines stay readable, and copying waits until the whole plan has arrived.',
      '',
      'Ask me to **compare** fares to get a table. Tables stream in row by row and keep their column layout while a row is still arriving, rather than flashing raw pipe characters until the row completes.',
      '',
      'Ask me to **search** for flights to see tool calls and sources. Each tool call shows its status while it runs and expands to its raw input and output, and the sources appear under the answer once it finishes.',
      '',
      'Try **simulating a failure** to see how a dropped connection is handled: the partial answer stays, the turn is marked as failed, and retry replays the prompt into the same message.',
      '',
      'Every reply also has copy, regenerate, feedback and share actions, and regenerating keeps the earlier versions available through the pager underneath.',
    ].join('\n'),
    followUps: ['Write an itinerary', 'Search for flights'],
  },
];

// Opening lines that make a regenerated turn visibly different from the one before it.
const REGENERATE_LEADS = [
  'Let me take another pass at this.',
  'Here is a different route.',
  'Trying once more, a little tighter.',
];

const HISTORY_SEEDS: { prompt: string; answer: string }[] = [
  {
    prompt: 'Why did my fare change overnight?',
    answer:
      'Seat inventory. As the cheaper fare classes sell out, only the next price up is left. Set a fare alert on the route, and most of those jumps stop surprising you.',
  },
  {
    prompt: 'Can I change seats after check-in?',
    answer:
      'Usually, yes, until boarding starts. Open the trip, tap your seat and pick from the live `Seat map`; paid seats and exit rows may need a fee or a quick check at the gate.',
  },
  {
    prompt: 'How early should I get to the airport?',
    answer:
      'Start at **two hours** for domestic, three for international. Add more for busy hubs, at the cost of extra coffee.',
  },
  {
    prompt: 'What happens if my connection is late?',
    answer:
      'On one ticket, the airline rebooks you. Until the inbound flight lands, the new time is a guess and Skyfy updates it as they post. Alerts catch most of that.',
  },
];

// Stable small hash so the same free-form prompt always picks the same general script.
const hashPrompt = (prompt: string) => {
  let hash = 0;
  for (let index = 0; index < prompt.length; index++) {
    hash = (hash + prompt.charCodeAt(index)) % 9973;
  }
  return hash;
};

export function buildUserMessage(
  text: string,
  attachments: AssistantAttachment[] = []
): AssistantPrompt {
  return {
    id: generateUniqueId(),
    role: 'user',
    text,
    attachments,
  };
}

export function buildReply(model: string): AssistantReply {
  return {
    id: generateUniqueId(),
    role: 'assistant',
    model,
    variants: [emptyTurn()],
    variantIndex: 0,
  };
}

/*
 * The script a prompt plays back, chosen by keyword. `variant` > 0 prefixes a rotating
 * opening line so each regeneration reads differently.
 */
export function pickScript(prompt: string, variant = 0): AssistantScript {
  const lower = prompt.toLowerCase();
  // The failure prompt only fails on its first attempt, so Retry can succeed.
  const base = /error|fail|drop/.test(lower)
    ? variant === 0
      ? FAILURE_SCRIPT
      : RECOVERED_SCRIPT
    : /hook|code|write/.test(lower)
      ? CODE_SCRIPT
      : /compare|table|strateg/.test(lower)
        ? TABLE_SCRIPT
        : /search|docs|find/.test(lower)
          ? RESEARCH_SCRIPT
          : GENERAL_SCRIPTS[hashPrompt(prompt) % GENERAL_SCRIPTS.length]!;

  if (variant === 0) return base;

  const lead = REGENERATE_LEADS[(variant - 1) % REGENERATE_LEADS.length]!;
  return { ...base, content: `${lead}\n\n${base.content}` };
}

/*
 * Finished question/answer pairs for the "load earlier" affordance. `offset` continues
 * the seed cycle so successive pages don't repeat the same pair back to back.
 */
export function buildHistory(pairs: number, offset = 0): AssistantMessage[] {
  return Array.from({ length: pairs }, (_, index): AssistantMessage[] => {
    const seed = HISTORY_SEEDS[(offset + index) % HISTORY_SEEDS.length]!;
    return [
      buildUserMessage(seed.prompt),
      {
        ...buildReply(ASSISTANT_MODELS[0]!),
        variants: [{ status: 'done', content: seed.answer }],
      },
    ];
  }).flat();
}

// Per-event delays (ms) before jitter: a thinking token, a content token, a tool round trip.
const FIRST_EVENT_MS = 350;
const THINKING_TOKEN_MS = 14;
const CONTENT_TOKEN_MS = 22;
const TOOL_START_MS = 160;
const TOOL_CALL_MS = 900;
// The writer's flush cadence: the last event gets one flush before the turn completes.
const FINISH_DELAY_MS = 50;

type StreamEvent =
  | { kind: 'thinking'; text: string }
  | { kind: 'toolStart'; name: string; input: string }
  | { kind: 'toolEnd'; output: string; failed: boolean }
  | { kind: 'content'; text: string }
  | { kind: 'fail'; error: string };

// Word-sized chunks that keep their whitespace, so code indentation and newlines survive.
const tokenize = (text: string) => text.match(/\s+|\S+/g) ?? [];

const buildEvents = (script: AssistantScript, thinking: boolean) => {
  const events: StreamEvent[] = [];

  if (thinking && script.thinking) {
    for (const text of tokenize(script.thinking)) {
      events.push({ kind: 'thinking', text });
    }
  }

  for (const call of script.toolCalls ?? []) {
    events.push({ kind: 'toolStart', name: call.name, input: call.input });
    events.push({
      kind: 'toolEnd',
      output: call.output,
      failed: call.fails === true,
    });
  }

  const tokens = tokenize(script.content);
  const cutoff =
    script.failAt === undefined
      ? tokens.length
      : Math.floor(tokens.length * script.failAt);
  for (const text of tokens.slice(0, cutoff)) {
    events.push({ kind: 'content', text });
  }

  if (script.failAt !== undefined) {
    events.push({
      kind: 'fail',
      error: script.error ?? 'Something went wrong.',
    });
  }

  return events;
};

const delayBefore = (event: StreamEvent) => {
  switch (event.kind) {
    case 'thinking':
      return THINKING_TOKEN_MS;
    case 'toolStart':
      return TOOL_START_MS;
    case 'toolEnd':
      return TOOL_CALL_MS;
    default:
      return CONTENT_TOKEN_MS;
  }
};

// 0.5x..1.5x, so tokens arrive in the uneven bursts a network actually delivers.
const jitter = (ms: number) => Math.round(ms * (0.5 + Math.random()));

export interface ScriptPlayback {
  stop: () => void;
}

// Plays a script into `writer` on timers, the way a network stream would feed it.
export function playScript(
  script: AssistantScript,
  writer: AssistantTurnWriter,
  { thinking }: { thinking: boolean }
): ScriptPlayback {
  const events = buildEvents(script, thinking);
  let cursor = 0;
  let toolCallId: string | undefined;
  let timer: ReturnType<typeof setTimeout> | undefined;

  const apply = (event: StreamEvent) => {
    switch (event.kind) {
      case 'thinking':
        writer.appendThinking(event.text);
        break;
      case 'toolStart':
        toolCallId = writer.startToolCall({
          name: event.name,
          input: event.input,
        });
        break;
      case 'toolEnd':
        if (toolCallId === undefined) break;
        if (event.failed) writer.failToolCall(toolCallId, event.output);
        else writer.completeToolCall(toolCallId, event.output);
        break;
      case 'content':
        writer.appendContent(event.text);
        break;
      case 'fail':
        writer.fail(event.error);
        break;
    }
  };

  const step = () => {
    const event = events[cursor];
    if (!event) {
      writer.complete({
        sources: script.sources?.map((source) => ({
          id: generateUniqueId(),
          title: source.title,
          domain: source.domain,
          url: `https://${source.domain}${source.path}`,
        })),
        followUps: script.followUps,
      });
      return;
    }
    cursor += 1;
    apply(event);
    if (writer.isFinished) return;

    const next = events[cursor];
    timer = setTimeout(
      step,
      next ? jitter(delayBefore(next)) : FINISH_DELAY_MS
    );
  };

  timer = setTimeout(step, FIRST_EVENT_MS);

  return {
    stop: () => {
      clearTimeout(timer);
      writer.stop();
    },
  };
}
