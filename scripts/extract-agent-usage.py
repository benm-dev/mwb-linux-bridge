#!/usr/bin/env python3
"""Summarize local agent token usage without printing prompt/response text."""

from __future__ import annotations

import argparse
import glob
import json
import os
from dataclasses import dataclass
from pathlib import Path
from typing import Any


OPENAI_PRICES = {
    # USD per 1M tokens. Cached input is treated separately when available.
    # Source: OpenAI API pricing page, update when models/prices change.
    "gpt-5.5": {"input": 5.00, "cached_input": 0.50, "output": 30.00},
    "gpt-5.4": {"input": 2.50, "cached_input": 0.25, "output": 15.00},
    "gpt-5.4-mini": {"input": 0.75, "cached_input": 0.075, "output": 4.50},
}

ANTHROPIC_PRICES = {
    # USD per 1M tokens. Cache read pricing is intentionally not estimated here.
    # Source: Anthropic Claude model/pricing docs, update when models/prices change.
    "claude-opus-4-8": {"input": 5.00, "cache_creation": 5.00, "output": 25.00},
}


@dataclass
class CodexSession:
    path: Path
    cwd: str
    session_id: str
    model: str
    input_tokens: int
    cached_input_tokens: int
    output_tokens: int
    reasoning_output_tokens: int
    total_tokens: int


@dataclass
class ClaudeSession:
    path: Path
    project: str
    model: str
    message_count: int
    input_tokens: int
    cache_creation_input_tokens: int
    cache_read_input_tokens: int
    output_tokens: int


def read_jsonl(path: Path) -> list[dict[str, Any]]:
    rows: list[dict[str, Any]] = []
    try:
        with path.open("r", encoding="utf-8") as handle:
            for line in handle:
                line = line.strip()
                if not line:
                    continue
                try:
                    rows.append(json.loads(line))
                except json.JSONDecodeError:
                    continue
    except OSError:
        return []
    return rows


def compact_number(value: int) -> str:
    if value >= 1_000_000:
        return f"{value / 1_000_000:.2f}M"
    if value >= 1_000:
        return f"{value / 1_000:.1f}K"
    return str(value)


def money(value: float | None) -> str:
    if value is None:
        return "n/a"
    if value < 0.01:
        return f"${value:.4f}"
    return f"${value:.2f}"


def short_session_id(session_id: str, path: Path) -> str:
    if session_id:
        return session_id[-8:]
    return path.stem[-8:]


def codex_price(model: str, input_tokens: int, cached_input_tokens: int, output_tokens: int) -> float | None:
    price = OPENAI_PRICES.get(model)
    if price is None:
        return None
    cached = min(cached_input_tokens, input_tokens)
    uncached = max(input_tokens - cached, 0)
    return (
        uncached * price["input"]
        + cached * price["cached_input"]
        + output_tokens * price["output"]
    ) / 1_000_000


def claude_price(
    model: str,
    input_tokens: int,
    cache_creation_input_tokens: int,
    output_tokens: int,
) -> float | None:
    price = ANTHROPIC_PRICES.get(model)
    if price is None:
        return None
    return (
        input_tokens * price["input"]
        + cache_creation_input_tokens * price["cache_creation"]
        + output_tokens * price["output"]
    ) / 1_000_000


def parse_codex_sessions(root: Path) -> list[CodexSession]:
    sessions: list[CodexSession] = []
    pattern = os.path.expanduser("~/.codex/sessions/**/*.jsonl")
    for name in glob.glob(pattern, recursive=True):
        path = Path(name)
        cwd = ""
        session_id = ""
        model = ""
        usage: dict[str, Any] | None = None

        for row in read_jsonl(path):
            payload = row.get("payload")
            if not isinstance(payload, dict):
                continue

            if row.get("type") == "session_meta":
                cwd = str(payload.get("cwd") or cwd)
                session_id = str(payload.get("id") or session_id)
                model = str(payload.get("model") or model)

            candidate_model = payload.get("model")
            if isinstance(candidate_model, str) and candidate_model:
                model = candidate_model

            collab = payload.get("collaboration_mode")
            if isinstance(collab, dict):
                settings = collab.get("settings")
                if isinstance(settings, dict):
                    candidate_model = settings.get("model")
                    if isinstance(candidate_model, str) and candidate_model:
                        model = candidate_model

            info = payload.get("info")
            if isinstance(info, dict) and isinstance(info.get("total_token_usage"), dict):
                usage = info["total_token_usage"]

        if not cwd or usage is None:
            continue
        try:
            cwd_path = Path(cwd).resolve()
        except OSError:
            cwd_path = Path(cwd)
        try:
            cwd_path.relative_to(root)
        except ValueError:
            if str(root) not in cwd:
                continue

        sessions.append(
            CodexSession(
                path=path,
                cwd=cwd,
                session_id=session_id,
                model=model or "unknown",
                input_tokens=int(usage.get("input_tokens") or 0),
                cached_input_tokens=int(usage.get("cached_input_tokens") or 0),
                output_tokens=int(usage.get("output_tokens") or 0),
                reasoning_output_tokens=int(usage.get("reasoning_output_tokens") or 0),
                total_tokens=int(usage.get("total_tokens") or 0),
            )
        )
    sessions.sort(key=lambda item: item.path.stat().st_mtime if item.path.exists() else 0, reverse=True)
    return sessions


def parse_claude_sessions() -> list[ClaudeSession]:
    sessions: list[ClaudeSession] = []
    pattern = os.path.expanduser("~/.claude/projects/-home-ben-Documents-Mouse-without-borders*/*.jsonl")
    for name in glob.glob(pattern):
        path = Path(name)
        project = path.parent.name.removeprefix("-home-ben-Documents-Mouse-without-borders").lstrip("-")
        project = project or "Mouse-without-borders"
        model = "unknown"
        message_count = 0
        input_tokens = 0
        cache_creation_input_tokens = 0
        cache_read_input_tokens = 0
        output_tokens = 0

        for row in read_jsonl(path):
            message = row.get("message")
            if not isinstance(message, dict):
                continue
            candidate_model = message.get("model")
            if isinstance(candidate_model, str) and candidate_model:
                model = candidate_model
            usage = message.get("usage")
            if not isinstance(usage, dict):
                continue
            message_count += 1
            input_tokens += int(usage.get("input_tokens") or 0)
            cache_creation_input_tokens += int(usage.get("cache_creation_input_tokens") or 0)
            cache_read_input_tokens += int(usage.get("cache_read_input_tokens") or 0)
            output_tokens += int(usage.get("output_tokens") or 0)

        if message_count:
            sessions.append(
                ClaudeSession(
                    path=path,
                    project=project,
                    model=model,
                    message_count=message_count,
                    input_tokens=input_tokens,
                    cache_creation_input_tokens=cache_creation_input_tokens,
                    cache_read_input_tokens=cache_read_input_tokens,
                    output_tokens=output_tokens,
                )
            )
    sessions.sort(key=lambda item: item.path.stat().st_mtime if item.path.exists() else 0, reverse=True)
    return sessions


def print_codex(sessions: list[CodexSession], limit: int) -> None:
    print("Codex local token totals:")
    if not sessions:
        print("  no Codex token records found for this workspace")
        return
    print("  worktree                         model              input  cached output reason total   est")
    for session in sessions[:limit]:
        worktree = Path(session.cwd).name
        est = codex_price(session.model, session.input_tokens, session.cached_input_tokens, session.output_tokens)
        print(
            f"  {worktree[:32]:32} {session.model[:18]:18} "
            f"{compact_number(session.input_tokens):>6} "
            f"{compact_number(session.cached_input_tokens):>6} "
            f"{compact_number(session.output_tokens):>6} "
            f"{compact_number(session.reasoning_output_tokens):>6} "
            f"{compact_number(session.total_tokens):>6} "
            f"{money(est):>7}"
        )


def print_claude(sessions: list[ClaudeSession], limit: int) -> None:
    print("Claude local token totals:")
    if not sessions:
        print("  no Claude token records found for this workspace")
        return
    print("  project                          model              input  create read   output messages est")
    for session in sessions[:limit]:
        est = claude_price(
            session.model,
            session.input_tokens,
            session.cache_creation_input_tokens,
            session.output_tokens,
        )
        print(
            f"  {session.project[:32]:32} {session.model[:18]:18} "
            f"{compact_number(session.input_tokens):>6} "
            f"{compact_number(session.cache_creation_input_tokens):>6} "
            f"{compact_number(session.cache_read_input_tokens):>6} "
            f"{compact_number(session.output_tokens):>6} "
            f"{session.message_count:>8} "
            f"{money(est):>7}"
        )
    print("  note: Claude est excludes cache-read discounts/costs unless priced above.")


def print_antigravity() -> None:
    print("Antigravity local token totals:")
    print("  no stable local token/credit records found; app quota is cloud-account data")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", default="/home/ben/Documents/Mouse without borders")
    parser.add_argument("--limit", type=int, default=8)
    args = parser.parse_args()

    root = Path(args.root).resolve()
    limit = max(args.limit, 1)

    print_codex(parse_codex_sessions(root), limit)
    print()
    print_claude(parse_claude_sessions(), limit)
    print()
    print_antigravity()
    print()
    print("Pricing notes: estimates use local token metadata plus configured public API rates; they are not provider invoices.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
