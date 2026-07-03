# How to Ask Good Questions — Student Interaction Templates
## STM32F4xx Embedded Systems Course

Learning to ask good questions is a skill as important as writing good code. A well-formed question gives the AI teaching assistant the context it needs to guide you effectively — without doing the thinking for you. The templates below are starting points, not rigid scripts. Read them, understand the intent behind each one, and adapt them to your specific situation. The goal is always to arrive at the AI with your own thinking already started.

---

## Template 1 — Asking for a concept explanation

Use this when you have encountered a concept in the theory class or in the reference manual and you want to deepen your understanding before applying it in code.

> "I am studying [concept] this week. I think it means [your current understanding], but I am not completely sure I have it right. Can you explain it at the level of a week [N] student, without using concepts from future weeks?"

**Why this works:** You are showing the AI what you already think, which allows it to correct misconceptions rather than starting from zero. You are also setting the scope explicitly — "week N student" — which prevents the AI from jumping ahead to concepts you haven't learned yet. Providing your current understanding, even if it's wrong, is far more useful than asking "what is X?" with no context.

---

## Template 2 — Requesting an ASCII diagram

Use this when you are struggling to visualize something spatial or structural — a register layout, a state machine, a memory map, a bitwise operation step by step, or a circuit connection.

> "I am having trouble visualizing [concept or operation]. Could you draw a simple ASCII diagram showing [what you want to see — for example: the bit layout of the MODER register for pin 5, or the states and transitions of my traffic light FSM]?"

**Why this works:** Some concepts are genuinely easier to understand visually than verbally. A register bit layout drawn in ASCII is often clearer than three paragraphs of description. Be specific about what you want the diagram to show — "the register" is vague, but "the MODER register showing which bits control pin 5 and what values mean input versus output" gives the AI enough detail to draw something truly useful.

---

## Template 3 — Asking for help debugging without getting the answer

Use this when your code compiles and runs but produces the wrong result, and you want guidance on finding the bug yourself rather than being told what it is.

> "My code compiles without errors but the behavior is wrong. I expected [what you expected to happen] but instead I observe [what is actually happening]. I have already checked [what you have already verified]. Can you help me think through what I might be missing, without telling me the answer directly?"

**Why this works:** This template forces you to do three things before asking for help — describe the expected behavior, describe the actual behavior, and list what you have already checked. Doing this often reveals the problem before the AI even responds. The explicit request to not receive the answer directly keeps the debugging work in your hands, which is where the learning happens.

---

## Template 4 — Verifying your understanding of a register operation

Use this when you have written a register operation and want to check whether your reasoning is correct before running the code.

> "I want to [describe what you want to achieve — for example: set pin 5 of GPIOA as a push-pull output]. I looked up the [register name] in the reference manual and I think I need to write [your proposed operation]. Is my reasoning correct? If not, can you tell me which part of my thinking is wrong without giving me the correct code?"

**Why this works:** You are presenting your work and your reasoning, not just asking for the answer. This allows the AI to evaluate your thinking process, not just your result. Even if the final code is wrong, a correct reasoning process means you are on the right path. Asking "which part of my thinking is wrong" rather than "what is the correct answer" keeps the intellectual work with you.

---

## Template 5 — Asking the AI to check your code style

Use this when you have written a complete function or module and want feedback on whether it follows the course coding standards before submitting.

> "I have written the following code. Can you review it for compliance with the course coding standards — file structure, naming conventions, comment style, and formatting — and point out anything that does not follow the rules? Please do not suggest changes to the logic, only to the style."

**Why this works:** By asking only for style feedback and explicitly excluding logic changes, you keep ownership of your implementation. The AI becomes a code style checker, not a code rewriter. This is a legitimate and useful way to use the tool — learning the standards by seeing them applied to your own code is more effective than reading them in a document.

---

## Template 6 — Asking for a step-by-step thinking guide

Use this when you are facing a task that feels overwhelming and you do not know where to start, but you want to find the starting point yourself rather than being handed a solution.

> "I need to [describe the task — for example: configure USART2 for 115200 baud on the STM32F446]. I do not know where to begin. Can you give me the first question I should ask myself, without telling me the answer? I want to work through the steps one at a time."

**Why this works:** This template explicitly asks for a question rather than an answer, which keeps the thinking process yours. Once you answer that first question, you can return with the next step. This back-and-forth process — one question at a time — mirrors the way an experienced engineer approaches an unfamiliar problem: methodically, not all at once.

---

## Template 7 — Connecting a new concept to something already learned

Use this when you have just encountered a new concept and you want to understand it by relating it to something you already know from previous weeks.

> "We just learned about [new concept] this week. Can you explain how it relates to [something from a previous week — for example: how DMA relates to the interrupt concept from week 5]? I want to understand the connection, not just the new concept in isolation."

**Why this works:** Learning is most durable when new knowledge connects to existing knowledge. This template asks the AI to build a bridge rather than introduce an island. Understanding how DMA is a more powerful evolution of interrupt-driven I/O, for example, is far more useful than understanding DMA as an isolated mechanism. Making these connections is exactly how the spiral curriculum is designed to work — each pass adds depth to what came before.

---

## Template 8 — Asking the rubber duck question

Use this when you are going to explain your code or your problem to the AI, but you want to use the conversation as a rubber duck debugging session — talking through your logic to find the error yourself.

> "I am going to explain my code to you as if you know nothing about what I am trying to do. Please listen and only ask me a question if something I say does not make sense or seems contradictory. Do not point out errors directly — just ask me to clarify the parts that seem unclear."

**Why this works:** The rubber duck technique works because explaining something out loud forces your brain to process it differently than when you read it silently. Errors that were invisible when reading suddenly become obvious when you have to articulate every step. The AI's role here is to be a patient listener who asks clarifying questions — not to find your bug for you, but to create the conditions where you find it yourself. Before typing this message, try saying your explanation out loud first. You may not even need to send it.

---

## Template 9 — Asking about a concept from a future week

Use this when your curiosity takes you beyond the current week's material and you want to explore ahead, while still being aware that the full explanation will come later.

> "I am curious about [future topic — for example: how interrupts work]. I am currently in week [N] so I know I will not fully understand it yet. Can you give me a one-sentence intuition for why this concept exists, without explaining how it works? I just want to know why it matters."

**Why this works:** This template acknowledges the boundary of your current knowledge and asks for something appropriate to your level — a "why" rather than a "how." A one-sentence intuition is enough to satisfy curiosity and maintain motivation without overwhelming you with concepts your foundation is not yet ready to support. It also signals to the AI that you are self-aware about where you are in the course, which helps it calibrate its response.

---

## Template 10 — Asking for a challenge

Use this when you have completed the week's exercises and want to go deeper into the current topic rather than jumping ahead to future material.

> "I have finished this week's exercises and I feel comfortable with [topic]. Can you give me a harder challenge that stays within week [N]'s concepts? I want something that pushes me deeper into what I already know, not something that requires concepts I have not learned yet."

**Why this works:** Curiosity is one of the most valuable assets an engineer can have — and the best way to channel it productively is to go deeper, not faster. A challenge that lives within your current knowledge forces you to combine, extend, and stress-test what you know. This is the difference between a student who finishes the lab early and sits idle, and one who uses that time to build genuine mastery. The AI can generate challenges tailored exactly to your current level because you have told it precisely where you are.
