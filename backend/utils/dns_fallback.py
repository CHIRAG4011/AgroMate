"""
AgroMate Resilient DNS Fallback
Provides transparent fallback using DNS-over-HTTPS (DoH) via 1.1.1.1 / 8.8.8.8
when the local router/system DNS resolver fails with socket.gaierror.
Ensures zero-downtime API connectivity across changing Wi-Fi, hotspot, and mobile networks.
"""

import socket
import urllib.request
import json
import logging

logger = logging.getLogger(__name__)

_orig_getaddrinfo = socket.getaddrinfo
_dns_cache = {}
_installed = False


def _resolve_doh(host: str) -> str:
    """Resolve hostname using Cloudflare / Google DoH JSON API."""
    if host in _dns_cache:
        return _dns_cache[host]

    # Don't try to resolve numeric IPs or localhost
    if host in ("localhost", "127.0.0.1", "::1") or host.replace(".", "").isdigit():
        return host

    providers = [
        f"https://1.1.1.1/dns-query?name={host}&type=A",
        f"https://dns.google/resolve?name={host}&type=A",
    ]

    for url in providers:
        try:
            req = urllib.request.Request(url, headers={"accept": "application/dns-json"})
            with urllib.request.urlopen(req, timeout=3.5) as resp:
                data = json.loads(resp.read().decode())
                for ans in data.get("Answer", []):
                    if ans.get("type") == 1:  # A record
                        ip = ans["data"]
                        _dns_cache[host] = ip
                        return ip
        except Exception:
            continue

    return ""


def patched_getaddrinfo(host, port, family=0, type=0, proto=0, flags=0):
    """Intercept getaddrinfo to provide DoH fallback on socket.gaierror."""
    try:
        return _orig_getaddrinfo(host, port, family, type, proto, flags)
    except socket.gaierror:
        # System resolver failed — attempt DoH fallback
        if isinstance(host, str):
            ip = _resolve_doh(host)
            if ip:
                logger.debug(f"[DNS-Fallback] Resolved {host} -> {ip} via DoH")
                return _orig_getaddrinfo(ip, port, family, type, proto, flags)
        raise


def install_dns_fallback():
    """Install the socket.getaddrinfo monkeypatch if not already installed."""
    global _installed
    if not _installed:
        socket.getaddrinfo = patched_getaddrinfo
        _installed = True
        logger.info("🛡️ AgroMate DNS-over-HTTPS fallback installed")

