# QmClient translation task

You are translating DDNet / QmClient UI and help text.

Rules:
- Return JSON only.
- Keep the original key and context unchanged.
- Respect DDNet and QmClient terminology.
- Do not invent product names or feature names.
- Do not add explanations outside JSON.
- Keep placeholders like %s, %d, %.2f, \\n unchanged.
- Keep punctuation and capitalization appropriate for the target language.
- If a term should stay in English, keep it in English.

JSON shape:
[
  {
    "key": "English key",
    "context": "Optional context",
    "translation": "Target translation",
    "notes": "Optional short note"
  }
]
