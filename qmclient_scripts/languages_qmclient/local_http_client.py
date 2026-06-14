#!/usr/bin/env python3
"""Minimal OpenAI-compatible local HTTP client for translation tooling."""

from __future__ import annotations

import json
import time
import urllib.error
import urllib.request
from dataclasses import dataclass


@dataclass(frozen=True)
class ChatMessage:
    role: str
    content: str


def build_chat_payload(
    model: str,
    messages: list[ChatMessage],
    *,
    temperature: float = 0.2,
) -> dict:
    return {
        "model": model,
        "messages": [
            {"role": message.role, "content": message.content} for message in messages
        ],
        "temperature": temperature,
    }


def extract_response_text(payload: dict) -> str:
    try:
        return payload["choices"][0]["message"]["content"]
    except (KeyError, IndexError, TypeError) as exc:
        raise ValueError("invalid chat completion response payload") from exc


class LocalHttpClient:
    def __init__(
        self,
        *,
        base_url: str,
        model: str,
        api_key: str = "",
        timeout_seconds: float = 60.0,
        max_retries: int = 2,
        retry_backoff_seconds: float = 1.0,
    ) -> None:
        self.base_url = base_url.rstrip("/")
        self.model = model
        self.api_key = api_key
        self.timeout_seconds = timeout_seconds
        self.max_retries = max_retries
        self.retry_backoff_seconds = retry_backoff_seconds

    def chat_completion(
        self,
        messages: list[ChatMessage],
        *,
        temperature: float = 0.2,
    ) -> str:
        payload = build_chat_payload(
            self.model, messages, temperature=temperature
        )
        raw = self._post_json("/chat/completions", payload)
        return extract_response_text(raw)

    def _post_json(self, path: str, payload: dict) -> dict:
        url = f"{self.base_url}{path}"
        body = json.dumps(payload).encode("utf-8")
        headers = {
            "Content-Type": "application/json",
        }
        if self.api_key:
            headers["Authorization"] = f"Bearer {self.api_key}"
        request = urllib.request.Request(url, data=body, headers=headers, method="POST")

        last_error: Exception | None = None
        for attempt in range(self.max_retries + 1):
            try:
                with urllib.request.urlopen(
                    request, timeout=self.timeout_seconds
                ) as response:
                    return json.loads(response.read().decode("utf-8"))
            except urllib.error.HTTPError as exc:
                if exc.code in {429, 500, 502, 503, 504} and attempt < self.max_retries:
                    last_error = exc
                    time.sleep(self.retry_backoff_seconds * (attempt + 1))
                    continue
                raise RuntimeError(
                    f"local HTTP request failed: {exc.code} {exc.reason}"
                ) from exc
            except urllib.error.URLError as exc:
                last_error = exc
                if attempt < self.max_retries:
                    time.sleep(self.retry_backoff_seconds * (attempt + 1))
                    continue
                raise RuntimeError(f"local HTTP request failed: {exc.reason}") from exc
            except json.JSONDecodeError as exc:
                raise RuntimeError("local HTTP response was not valid JSON") from exc

        raise RuntimeError("local HTTP request failed") from last_error
