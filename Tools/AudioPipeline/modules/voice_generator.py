"""
Voice Generator — AI text-to-speech for character voice lines.
Supports ElevenLabs and Azure Cognitive Services.
"""

import io
import json
import os
import time
from abc import ABC, abstractmethod
from pathlib import Path
from typing import Optional

import requests


class VoiceProvider(ABC):
    @abstractmethod
    def generate_line(self, text: str, voice_id: str,
                      language: str = "en") -> Optional[bytes]:
        pass

    @abstractmethod
    def is_available(self) -> bool:
        pass


class ElevenLabsVoiceProvider(VoiceProvider):
    """ElevenLabs Text-to-Speech API."""

    def __init__(self, config: dict):
        self.config = config
        self._api_key = os.environ.get(config.get("env_key", "ELEVENLABS_API_KEY"))
        self.base_url = config.get("base_url", "https://api.elevenlabs.io/v1")

        # Default voice IDs (ElevenLabs pre-made voices)
        self.voice_map = {
            "team_leader": "pNInz6obpgDQGcFmaJgB",       # Adam
            "rifleman": "ErXwobaYiN019PkySvjV",           # Antoni
            "automatic_rifleman": "VR6AewLTigWG4xSOukaG",  # Arnold
            "grenadier": "TxGEqnHWrfWFTfGW9XjX",          # Josh
            "marksman": "yoZ06aMxZJJ28mfd3POQ",           # Sam
            "officer": "onwK4e9ZLuTAKqWW03F9",            # Chinese male
            "nco": "onwK4e9ZLuTAKqWW03F9",
            "soldier_1": "onwK4e9ZLuTAKqWW03F9",
            "soldier_2": "onwK4e9ZLuTAKqWW03F9",
        }

    def is_available(self) -> bool:
        return bool(self._api_key) and self.config.get("enabled", False)

    def generate_line(self, text: str, voice_id: str,
                      language: str = "en") -> Optional[bytes]:
        if not self.is_available():
            return None

        vid = self.voice_map.get(voice_id, voice_id)
        url = f"{self.base_url}/text-to-speech/{vid}"
        headers = {
            "xi-api-key": self._api_key,
            "Accept": "audio/mpeg",
            "Content-Type": "application/json",
        }
        payload = {
            "text": text,
            "model_id": "eleven_multilingual_v2",
            "voice_settings": {
                "stability": 0.5,
                "similarity_boost": 0.75,
                "style": 0.3,
            },
        }

        try:
            resp = requests.post(url, headers=headers, json=payload, timeout=60)
            resp.raise_for_status()
            return resp.content
        except requests.RequestException as e:
            print(f"[ElevenLabs Voice] Failed: {e}")
            return None


class AzureTTSProvider(VoiceProvider):
    """Azure Cognitive Services Text-to-Speech."""

    def __init__(self, config: dict):
        self.config = config
        self._api_key = os.environ.get(config.get("env_key", "AZURE_SPEECH_KEY"))
        self.region = config.get("region", "eastus")

        self.voice_map = {
            "team_leader": "en-US-GuyNeural",
            "rifleman": "en-US-DavisNeural",
            "automatic_rifleman": "en-US-JasonNeural",
            "grenadier": "en-US-TonyNeural",
            "marksman": "en-US-BrandonNeural",
            "officer": "zh-CN-YunxiNeural",
            "nco": "zh-CN-YunjianNeural",
            "soldier_1": "zh-CN-YunzeNeural",
            "soldier_2": "zh-CN-YunyeNeural",
        }

    def is_available(self) -> bool:
        return bool(self._api_key) and self.config.get("enabled", False)

    def generate_line(self, text: str, voice_id: str,
                      language: str = "en") -> Optional[bytes]:
        if not self.is_available():
            return None

        voice_name = self.voice_map.get(voice_id, "en-US-GuyNeural")
        lang = "zh-CN" if language == "zh" else "en-US"

        url = f"https://{self.region}.tts.speech.microsoft.com/cognitiveservices/v1"
        headers = {
            "Ocp-Apim-Subscription-Key": self._api_key,
            "Content-Type": "application/ssml+xml",
            "X-Microsoft-OutputFormat": "riff-48khz-16bit-mono-pcm",
        }

        ssml = f"""<speak version='1.0' xml:lang='{lang}'>
  <voice xml:lang='{lang}' name='{voice_name}'>
    <prosody rate='medium' pitch='medium'>{text}</prosody>
  </voice>
</speak>"""

        try:
            resp = requests.post(url, headers=headers, data=ssml.encode("utf-8"),
                                 timeout=60)
            resp.raise_for_status()
            return resp.content
        except requests.RequestException as e:
            print(f"[Azure TTS] Failed: {e}")
            return None


class VoiceGenerator:
    """Manages voice providers and generates all character voice lines."""

    def __init__(self, config_path: str):
        with open(config_path) as f:
            config = json.load(f)

        self.providers: list[VoiceProvider] = []
        voice_cfg = config.get("voice_generation", {})

        if "elevenlabs" in voice_cfg:
            self.providers.append(ElevenLabsVoiceProvider(voice_cfg["elevenlabs"]))
        if "azure_tts" in voice_cfg:
            self.providers.append(AzureTTSProvider(voice_cfg["azure_tts"]))

    def get_available_provider(self) -> Optional[VoiceProvider]:
        for p in self.providers:
            if p.is_available():
                return p
        return None

    def generate_line(self, text: str, role: str,
                      language: str = "en") -> Optional[bytes]:
        """Generate a single voice line."""
        provider = self.get_available_provider()
        if not provider:
            return None
        return provider.generate_line(text, role, language)

    def generate_all_friendly(self, manifest: dict,
                               output_dir: Path) -> dict[str, Path]:
        """Generate all friendly squad voice lines."""
        provider = self.get_available_provider()
        if not provider:
            print("[VoiceGenerator] No voice provider available")
            return {}

        generated = {}
        squad_cfg = manifest["friendly_squad"]

        for role in squad_cfg["roles"]:
            for context, lines_by_role in squad_cfg["contexts"].items():
                role_lines = lines_by_role.get(role, [])
                if not role_lines:
                    continue

                for stress in ["calm", "normal", "stressed"]:
                    line = role_lines[0]  # Use first variant
                    line_id = f"{role}_{context}_{stress}"

                    audio = provider.generate_line(line, role, "en")
                    if audio:
                        out_path = output_dir / "friendly" / role / f"VO_{line_id}.wav"
                        out_path.parent.mkdir(parents=True, exist_ok=True)
                        with open(out_path, "wb") as f:
                            f.write(audio)
                        generated[line_id] = out_path
                        print(f"[Voice] Generated: {line_id}")

                    time.sleep(0.5)

        return generated

    def generate_all_enemy(self, manifest: dict,
                            output_dir: Path) -> dict[str, Path]:
        """Generate all enemy PLA voice lines."""
        provider = self.get_available_provider()
        if not provider:
            return {}

        generated = {}
        enemy_cfg = manifest["enemy_pla"]

        for context, ctx_data in enemy_cfg["contexts"].items():
            text = ctx_data["script_zh"]

            for speaker in enemy_cfg["speakers"]:
                line_id = f"pla_{speaker}_{context}"

                audio = provider.generate_line(text, speaker, "zh")
                if audio:
                    out_path = output_dir / "enemy" / speaker / f"VO_{line_id}.wav"
                    out_path.parent.mkdir(parents=True, exist_ok=True)
                    with open(out_path, "wb") as f:
                        f.write(audio)
                    generated[line_id] = out_path
                    print(f"[Voice] Generated: {line_id}")

                time.sleep(0.5)

        return generated
