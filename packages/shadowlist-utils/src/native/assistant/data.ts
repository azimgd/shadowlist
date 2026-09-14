import { AVATAR_COLORS, generateUniqueId } from 'shadowlist-utils';

/*
 * Assistant demo data: the message shapes, the scripted replies a stream plays back, and
 * the generators that seed history. There is no model behind it -- a prompt picks a
 * script by keyword and stream.ts plays it out token by token, which exercises the list
 * exactly the way a real streaming chat does.
 */

export interface AssistantAttachment {
  id: string;
  kind: 'image' | 'file';
  name: string;
  detail: string;
  color: string;
}

/*
 * 'stopped' is distinct from 'error' on purpose: a tool still running when the reader hits
 * stop was interrupted by them, not by a failure, and painting it red says the opposite.
 */
export type AssistantToolStatus = 'running' | 'done' | 'error' | 'stopped';

export interface AssistantToolInvocation {
  id: string;
  name: string;
  input: string;
  output: string;
  status: AssistantToolStatus;
}

export interface AssistantSource {
  id: string;
  title: string;
  domain: string;
  url: string;
}

export type AssistantTurnStatus = 'streaming' | 'done' | 'stopped' | 'error';

// One generated answer. A reply keeps every regeneration as its own turn.
export interface AssistantTurn {
  thinking: string;
  // Wall time of the thinking phase; 0 until the first content token lands.
  thinkingMs: number;
  toolCalls: AssistantToolInvocation[];
  content: string;
  sources: AssistantSource[];
  followUps: string[];
  status: AssistantTurnStatus;
  error: string;
}

export type AssistantFeedback = 'good' | 'bad' | null;

// Named ...Prompt so it does not shadow the <AssistantUserMessage /> component it feeds.
export interface AssistantPrompt {
  id: string;
  role: 'user';
  text: string;
  attachments: AssistantAttachment[];
}

export interface AssistantReply {
  id: string;
  role: 'assistant';
  // The prompt that produced it, kept so regenerate and retry can replay it.
  prompt: string;
  model: string;
  variants: AssistantTurn[];
  variantIndex: number;
  feedback: AssistantFeedback;
}

// Trailing 1pt row. Whether it is on screen is how the screen knows the reader is at the bottom.
export interface AssistantEndMarker {
  id: string;
  role: 'end';
}

export type AssistantMessage =
  | AssistantPrompt
  | AssistantReply
  | AssistantEndMarker;

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
  // Fraction of `content` streamed before the connection "drops". Omit for a clean finish.
  failAt?: number;
  error?: string;
}

export const ASSISTANT_END_ID = 'assistant-end';

export const ASSISTANT_END_MARKER: AssistantEndMarker = {
  id: ASSISTANT_END_ID,
  role: 'end',
};

export const ASSISTANT_MODELS = ['Balanced', 'Swift', 'Deep'];

export const ASSISTANT_SUGGESTIONS: { title: string; prompt: string }[] = [
  {
    title: 'Write a hook',
    prompt: 'Write a hook that coalesces a token stream',
  },
  {
    title: 'Compare strategies',
    prompt: 'Compare list virtualization strategies in a table',
  },
  {
    title: 'Search the docs',
    prompt: 'Search the docs for getElementSizeSpec',
  },
  {
    title: 'Simulate a failure',
    prompt: 'Simulate a dropped connection error',
  },
];

const ATTACHMENT_SEEDS: Omit<AssistantAttachment, 'id' | 'color'>[] = [
  { kind: 'image', name: 'screenshot.png', detail: '1.4 MB' },
  { kind: 'file', name: 'profile-trace.json', detail: '312 KB' },
  { kind: 'file', name: 'ChatScreen.tsx', detail: '6 KB' },
];

const CODE_SCRIPT: AssistantScript = {
  thinking:
    'They want a hook for streamed tokens. Rendering once per token is the usual mistake: at 40 tokens a second that is 40 renders a second for text nobody reads that fast. Buffer in a ref, flush on an interval, and keep the subscription inside the one row that streams.',
  content: [
    "Here's a hook that coalesces a token stream into **one render per flush** instead of one render per token. It is small, but it is the piece that decides whether a streaming reply feels smooth or stutters on a mid-range phone.",
    '',
    '```tsx',
    'export function useCoalescedText(source: TokenSource, flushMs = 50) {',
    "  const [text, setText] = useState('');",
    "  const buffer = useRef('');",
    '',
    '  useEffect(() => {',
    '    const unsubscribe = source.subscribe((token) => {',
    '      buffer.current += token;',
    '    });',
    '    const id = setInterval(() => {',
    '      if (!buffer.current) return;',
    '      const chunk = buffer.current;',
    "      buffer.current = '';",
    '      setText((prev) => prev + chunk);',
    '    }, flushMs);',
    '    return () => {',
    '      unsubscribe();',
    '      clearInterval(id);',
    '    };',
    '  }, [source, flushMs]);',
    '',
    '  return text;',
    '}',
    '```',
    '',
    'The first thing to notice is where tokens land. They go into a ref, not into state, so a token arriving never schedules a render on its own. A network can deliver dozens of tokens in a single burst after a pause, and with this shape that burst costs one string append per token and nothing else.',
    '',
    'The interval is what turns that buffer into renders. Every `flushMs` it checks whether anything arrived, and only then moves the buffered text into state. At the default of 50 ms that caps the reply at about twenty renders a second, however fast the model is generating.',
    '',
    'Twenty a second sounds low until you compare it to reading speed. Nobody reads faster than roughly ten updates a second, so the flush loses nothing you can see, while halving or quartering the work compared to rendering every token. On slower devices you can raise the interval further before anyone notices.',
    '',
    'Scope matters as much as cadence. Call this hook inside the row that renders the streaming message, not in the screen that owns the list. If the screen owns the text, every flush re-renders the screen and, through it, the list; if the row owns it, a flush wakes exactly one component.',
    '',
    'Cleanup is the last detail worth checking. The effect unsubscribes and clears the interval when the source changes or the row unmounts, so a stopped or abandoned stream never keeps a timer alive in the background. Add an abort signal to the source if you also want the network request itself cancelled.',
  ].join('\n'),
  followUps: ['Add cancellation', 'Make it a store instead'],
};

const TABLE_SCRIPT: AssistantScript = {
  thinking:
    'Three families cover it: windowing, recycling, and geometry kept in a native core. A table first, then the one trade-off that decides each.',
  content: [
    '## Virtualization strategies',
    '',
    'Every long list on mobile ends up choosing between the same three approaches. They differ less in what they render than in where the geometry lives and what happens when a new row scrolls into view.',
    '',
    '| Strategy | Mounted rows | Scroll cost | Best for |',
    '| --- | --- | --- | --- |',
    '| Windowing | Visible + overscan | Mount on entry | Simple feeds |',
    '| Recycling | Fixed pool | Rebind on entry | Uniform rows |',
    '| Native core | Visible + overscan | Geometry in C++ | Chat, long lists |',
    '',
    '**Windowing** keeps only the rows near the viewport mounted and mounts new ones as they approach. It is the easiest model to reason about, because a row is just a component with ordinary state, but every row entering the window pays a full mount, and fast flings can outrun it.',
    '',
    "**Recycling** keeps a fixed pool of cells and rebinds them to new data as they scroll off one edge and onto the other. It is the fastest option for identical rows, since nothing is created while scrolling, but variable heights and per-row state are awkward: a recycled cell carries the last row's state until you reset it.",
    '',
    '**A native core** keeps offsets, measurement and anchoring out of JavaScript entirely. JS still renders the rows, but deciding which rows exist, where they sit and how the scroll position is corrected happens next to the layout pass. That is what makes *streaming* rows cheap: a growing reply is an O(1) change to geometry that never crosses the bridge.',
    '',
    "Chat is the hardest case for all three, because it combines variable heights, prepended history and a row that changes size many times a second. Windowing re-measures, recycling fights the height changes, and only an approach that owns anchoring can keep the reader's position stable through all of it.",
    '',
    '---',
    '',
    'Pick by row shape first, then by list length. Uniform rows favour recycling, simple feeds are fine with windowing, and anything that grows, streams or prepends benefits from keeping geometry native.',
  ].join('\n'),
  followUps: ['Which one handles images?', 'Show a benchmark'],
};

const RESEARCH_SCRIPT: AssistantScript = {
  thinking:
    'Look up how getElementSizeSpec is documented before answering, then read the page that covers streaming rows.',
  toolCalls: [
    {
      name: 'search_docs',
      input: '{ "query": "getElementSizeSpec streaming" }',
      output: '3 results · getElementSizeSpec, ElementSizeSpec, streaming rows',
    },
    /*
     * Deliberately fails, so the tool error UI is reachable from a prompt. The answer is
     * written from the search result above it either way, which is what a real assistant
     * does when one fetch of several times out.
     */
    {
      name: 'read_page',
      input: '{ "url": "docs.example.com/list/measure" }',
      output: '504 · upstream timeout',
      fails: true,
    },
  ],
  content: [
    '`getElementSizeSpec` describes a row **before** it renders, so native can size it ahead of time instead of estimating and correcting. The docs frame it as a prediction channel: you tell the list what a row will measure to, and the real layout pass later confirms it.',
    '',
    "The shape it returns is an `ElementSizeSpec`: the row's text, the font attributes that affect wrapping, and insets for everything around the text such as padding, an avatar column or a fixed header strip. Native measures that text through the same engine the real layout uses, so a correct spec produces exactly the height the row would have had.",
    '',
    'Return a spec only for rows whose height is a function of their text. Anything else, such as images, async content or Markdown with code blocks and tables, should return `null`. The list then estimates those rows and measures them normally, and mixing described and undescribed rows in one list is expected.',
    '',
    "The row that is **streaming** is the important exception. It is always mounted, and a mounted row's real measurement outranks any prediction unconditionally, so describing it only produces work that is thrown away. Worse, its text changes every flush, which mints a new entry in the text measurement cache each time and evicts the entries other rows depend on.",
    '',
    'Keep the function referentially stable with `useCallback` or define it at module level. It is a memo dependency, so a new function identity on every render rebuilds the whole prediction window, which defeats the purpose on exactly the screens that need it most.',
    '',
    'For a chat, that leaves a simple rule: describe plain-text prompts exactly, return `null` for replies, and never describe the row that is currently streaming. See [ElementSizeSpec](https://docs.example.com/list/measure) for the full field list and the notes on percentage widths.',
  ].join('\n'),
  sources: [
    {
      title: 'getElementSizeSpec',
      domain: 'docs.example.com',
      path: '/list/measure',
    },
    {
      title: 'ElementSizeSpec reference',
      domain: 'docs.example.com',
      path: '/list/spec',
    },
    {
      title: 'Streaming rows',
      domain: 'guides.example.com',
      path: '/streaming',
    },
  ],
  followUps: ['Show an ElementSizeSpec example', 'What about images?'],
};

const FAILURE_SCRIPT: AssistantScript = {
  content: [
    'Starting a long answer so there is plenty on screen when the connection drops. A real stream can fail at any point, and the interface has to leave the reader with something coherent rather than a half-rendered block and no way forward.',
    '',
    'The first rule is to keep whatever already arrived. The partial text stays exactly where it stopped, with its formatting intact, because the reader may already have found the part they needed before the failure.',
    '',
    'The second rule is to say clearly that the answer is incomplete. A reply that simply stops looks finished, and the reader will trust it as if it were. Marking the turn as failed, with a short reason, removes that ambiguity.',
    '',
    'The third rule is to make recovery one tap. Retry replays the same prompt into the same message instead of appending a new exchange, so the conversation still reads as one question and one answer once it succeeds.',
    '',
    'Everything past this point would have covered how to resume a stream from the last received token, and how to tell a timeout from a server error so the retry can wait an appropriate amount of time first.',
    '',
    'It would also have covered keeping retries idempotent on the server, so replaying a prompt never double-charges or duplicates any side effects a tool call may have had.',
  ].join('\n'),
  failAt: 0.45,
  error: 'The connection was interrupted.',
};

// What the failure prompt plays on a retry or regeneration: the connection holds this time.
const RECOVERED_SCRIPT: AssistantScript = {
  content: [
    'Reconnected -- here is the whole answer this time.',
    '',
    'A dropped stream keeps its partial text and marks the turn as failed, so the reader never mistakes an interrupted answer for a finished one. That state is deliberately visible: a quiet notice under the text and a retry button, rather than an error that replaces the content.',
    '',
    'Retry replays the original prompt into the same message instead of appending a new one. The failed version is replaced in place, so once the retry succeeds the conversation reads as a single question and a single answer, with no orphaned half-reply above it.',
    '',
    'Regenerate is different on purpose. It keeps the previous version and adds a new one, and the pager under the reply lets you move between them. Retry fixes a failure; regenerate asks for an alternative.',
    '',
    "Behind both, the list does very little. The reply's data changes once when the new attempt starts and once when it ends, and every token in between only touches the row that is streaming, so a retry costs the same as any other reply.",
    '',
    'For a production client, add backoff before retrying automatically, resume from the last received token when the server supports it, and surface the difference between a network failure and a refusal so the reader knows whether trying again can help.',
    '',
    'Tool calls follow the same rule. A tool that was still running when the connection dropped is marked as failed rather than left spinning, and the retry runs it again from scratch, so a stale result from the interrupted attempt can never leak into the answer that replaces it.',
  ].join('\n'),
  followUps: ['Simulate another failure'],
};

const GENERAL_SCRIPTS: AssistantScript[] = [
  {
    content: [
      'Short answer: keep the list data **stable while a reply streams**. The rest of this is why that one rule matters so much.',
      '',
      'A naive streaming chat stores the reply text in the message array and replaces the array on every token. That gives the list a new data identity forty times a second, and a virtualized list treats a new identity as "something changed": it re-extracts keys, rebuilds its lookup tables and re-describes the rows around the viewport.',
      '',
      'Commit the array once when the stream starts and once when it ends instead. In between, let the streaming row read its text from a small external store that only that row subscribes to. The list sees no data change at all, so it does no per-token work.',
      '',
      'The row still re-renders, and it should. Its height changes as text arrives, and the layout pass reports that to the list, which only has to move the rows after it. For the newest message in a chat that is nothing, because nothing comes after it.',
      '',
      'Coalescing sits on top of that. Tokens accumulate in a buffer and the store publishes a snapshot every 50 ms or so, which caps renders at a rate people cannot out-read and gives each frame room to breathe.',
      '',
      'Once the stream ends, the final text is committed to the array in one update, the store entry is released after that update has rendered, and the row switches from its live snapshot to its committed data without a visible frame in between.',
    ].join('\n'),
    followUps: ['Why does that help?', 'Show the store'],
  },
  {
    thinking:
      'General question. Keep it concrete and walk through the two behaviours people notice first.',
    content: [
      'Two things matter most for a chat that feels fast, and neither is raw rendering speed.',
      '',
      'The first is **coalescing tokens**. Models and networks deliver text in uneven bursts, and rendering each token as it lands turns every burst into a flurry of layout passes. Flushing every ~50 ms smooths that into a steady rhythm the device can keep up with.',
      '',
      'The second is **following the bottom only while the reader is there**. While someone is watching the newest text arrive, the view should keep the latest line in sight. The moment they scroll up to reread something, it should stop moving entirely.',
      '',
      'That second behaviour is where most chat interfaces feel broken. If the view keeps pulling back to the bottom while you are dragging, you end up fighting the list for control, and every new paragraph yanks the text you were reading out from under your finger.',
      '',
      'The fix is to treat any upward scroll as intent. Release following immediately, hold the visible content still while the reply keeps growing below, and show a small button that jumps back to the latest text when the reader is ready.',
      '',
      'Scrolling all the way back down should resume following on its own, so the button is a shortcut rather than the only way back. Together those rules make streaming feel calm: the text arrives where you are looking, and nowhere else.',
    ].join('\n'),
    followUps: ['How do I detect scrolling up?'],
  },
  {
    content: [
      'Happy to help. This conversation is a template, so each kind of question shows off a different part of it.',
      '',
      'Ask for **code** to get a fenced block with a language label and a copy button. The block scrolls horizontally instead of wrapping, so long lines stay readable, and copying waits until the closing fence has arrived.',
      '',
      'Ask to **compare** something to get a table. Tables stream in row by row and keep their column layout while a row is still arriving, rather than flashing raw pipe characters until the row completes.',
      '',
      'Ask to **search** for something to see tool calls and sources. Each tool call shows its status while it runs and expands to its raw input and output, and the sources appear under the answer once it finishes.',
      '',
      'Try **simulating a failure** to see how a dropped connection is handled: the partial answer stays, the turn is marked as failed, and retry replays the prompt into the same message.',
      '',
      'Every reply also has copy, regenerate, feedback and share actions, and regenerating keeps the earlier versions available through the pager underneath.',
    ].join('\n'),
    followUps: ['Write a hook', 'Search the docs'],
  },
];

// Opening lines that make a regenerated turn visibly different from the one before it.
const REGENERATE_LEADS = [
  'Let me take another pass at this.',
  'Here is a different angle.',
  'Trying once more, a little tighter.',
];

const HISTORY_SEEDS: { prompt: string; answer: string }[] = [
  {
    prompt: 'Why does my list jump when images load?',
    answer:
      'Rows below the image move when it reports its real height. Give the image a fixed aspect ratio so its height is known up front, and the jump disappears.',
  },
  {
    prompt: 'Should a chat list be inverted?',
    answer:
      'Only if inverting is a layout policy rather than a `scaleY(-1)` transform. Transforms flip accessibility order and sticky headers; a policy keeps data chronological.',
  },
  {
    prompt: 'How big should overscan be?',
    answer:
      'Start at **one viewport** each way. Raise it for fast flings, and pair it with a smaller materialization band so the native view count stays low.',
  },
  {
    prompt: 'What makes scrollToIndex land late?',
    answer:
      'Estimated heights. Until rows above the target are measured, the offset is a guess and the scroll corrects as they arrive. Predicted sizes remove most of that.',
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

export function emptyTurn(): AssistantTurn {
  return {
    thinking: '',
    thinkingMs: 0,
    toolCalls: [],
    content: '',
    sources: [],
    followUps: [],
    status: 'streaming',
    error: '',
  };
}

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

export function buildReply(prompt: string, model: string): AssistantReply {
  return {
    id: generateUniqueId(),
    role: 'assistant',
    prompt,
    model,
    variants: [emptyTurn()],
    variantIndex: 0,
    feedback: null,
  };
}

// One attachment, cycling the seed set; a fresh id keeps repeated picks distinct.
export function buildAttachment(index: number): AssistantAttachment {
  return {
    id: generateUniqueId(),
    color: AVATAR_COLORS[(index * 3) % AVATAR_COLORS.length]!,
    ...ATTACHMENT_SEEDS[index % ATTACHMENT_SEEDS.length]!,
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
  return Array.from({ length: pairs }, (_, index) => {
    const seed = HISTORY_SEEDS[(offset + index) % HISTORY_SEEDS.length]!;
    const reply = buildReply(seed.prompt, ASSISTANT_MODELS[0]!);
    const turn: AssistantTurn = {
      ...emptyTurn(),
      content: seed.answer,
      status: 'done',
    };
    return [
      buildUserMessage(seed.prompt),
      { ...reply, variants: [turn] },
    ] as AssistantMessage[];
  }).flat();
}
