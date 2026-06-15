#!/usr/bin/env python3
"""Minimal OpenAI-compatible HTTP client for translation tooling."""

from __future__ import annotations

import http.client
import json
import time
import urllib.error
import urllib.parse
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
    max_tokens: int | None = None,
    extra_body: dict | None = None,
) -> dict:
    payload = {
        "model": model,
        "messages": [
            {"role": message.role, "content": message.content} for message in messages
        ],
        "temperature": temperature,
    }
    if max_tokens is not None:
        payload["max_tokens"] = max_tokens
    if extra_body:
        payload.update(extra_body)
    return payload


def extract_response_text(payload: dict) -> str:
    try:
        message = payload["choices"][0]["message"]
    except (KeyError, IndexError, TypeError) as exc:
        raise ValueError("invalid chat completion response payload") from exc
    content = message.get("content")
    if isinstance(content, str) and content:
        return content
    if message.get("reasoning_content"):
        raise ValueError(
            "chat completion returned reasoning_content without final content; "
            "increase --max-tokens or disable thinking"
        )
    raise ValueError("invalid chat completion response payload")


def should_bypass_proxy(url: str) -> bool:
    host = urllib.parse.urlparse(url).hostname or ""
    return host.lower() in {"127.0.0.1", "localhost", "::1"}


def build_request_headers(api_key: str = "") -> dict[str, str]:
    headers = {
        "Content-Type": "application/json",
        "User-Agent": "QmClient-i18n/1.0",
    }
    if api_key:
        headers["Authorization"] = f"Bearer {api_key}"
    return headers


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
        max_tokens: int | None = None,
        chat_extra_body: dict | None = None,
    ) -> None:
        self.base_url = base_url.rstrip("/")
        self.model = model
        self.api_key = api_key
        self.timeout_seconds = timeout_seconds
        self.max_retries = max_retries
        self.retry_backoff_seconds = retry_backoff_seconds
        self.max_tokens = max_tokens
        self.chat_extra_body = chat_extra_body or {}

    def chat_completion(
        self,
        messages: list[ChatMessage],
        *,
        temperature: float = 0.2,
        max_tokens: int | None = None,
    ) -> str:
        payload = build_chat_payload(
            self.model,
            messages,
            temperature=temperature,
            max_tokens=max_tokens if max_tokens is not None else self.max_tokens,
            extra_body=self.chat_extra_body,
        )
        raw = self._post_json("/chat/completions", payload)
        return extract_response_text(raw)

    def _post_json(self, path: str, payload: dict) -> dict:
        url = f"{self.base_url}{path}"
        body = json.dumps(payload).encode("utf-8")
        request = urllib.request.Request(
            url, data=body, headers=build_request_headers(self.api_key), method="POST"
        )
        opener = (
            urllib.request.build_opener(urllib.request.ProxyHandler({}))
            if should_bypass_proxy(url)
            else None
        )

        last_error: Exception | None = None
        for attempt in range(self.max_retries + 1):
            try:
                open_request = opener.open if opener else urllib.request.urlopen
                with open_request(request, timeout=self.timeout_seconds) as response:
                    return json.loads(response.read().decode("utf-8"))
            except urllib.error.HTTPError as exc:
                if exc.code in {429, 500, 502, 503, 504} and attempt < self.max_retries:
                    last_error = exc
                    time.sleep(self.retry_backoff_seconds * (attempt + 1))
                    continue
                detail = exc.read().decode("utf-8", errors="replace")
                raise RuntimeError(
                    f"local HTTP request failed: {exc.code} {exc.reason}: {detail}"
                ) from exc
            except (urllib.error.URLError, http.client.HTTPException, TimeoutError, OSError) as exc:
                last_error = exc
                if attempt < self.max_retries:
                    time.sleep(self.retry_backoff_seconds * (attempt + 1))
                    continue
                raise RuntimeError(f"local HTTP request failed: {exc}") from exc
            except json.JSONDecodeError as exc:
                raise RuntimeError("local HTTP response was not valid JSON") from exc

        raise RuntimeError("local HTTP request failed") from last_error
