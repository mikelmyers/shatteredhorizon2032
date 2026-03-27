"""Shattered Horizon 2032 — Art Pipeline CLI.

Entry point: `python -m pipeline <command>`
"""

from __future__ import annotations

import json
import uuid
from datetime import datetime, timezone
from pathlib import Path
from typing import Optional

import typer
from rich.console import Console
from rich.table import Table
from rich.panel import Panel
from rich.text import Text

PIPELINE_ROOT = Path(__file__).parent
console = Console()

app = typer.Typer(
    name="pipeline",
    help="Shattered Horizon 2032 — AI Art Pipeline System",
    no_args_is_help=True,
)
generate_app = typer.Typer(help="Generate assets")
style_app = typer.Typer(help="Style consistency checking")
inventory_app = typer.Typer(help="Asset inventory management")
costs_app = typer.Typer(help="Cost tracking and budget management")

app.add_typer(inventory_app, name="inventory")
app.add_typer(costs_app, name="costs")
app.add_typer(style_app, name="style")


# ---------------------------------------------------------------------------
# Lazy loaders — avoid import-time DB creation
# ---------------------------------------------------------------------------

def _get_tracker():
    from pipeline.costs.tracker import CostTracker
    return CostTracker(
        db_path=PIPELINE_ROOT / "costs" / "ledger.db",
        tools_config_path=PIPELINE_ROOT / "costs" / "tools.json",
    )


def _get_inventory():
    from pipeline.inventory.db import InventoryDB
    return InventoryDB(db_path=PIPELINE_ROOT / "pipeline.db")


def _get_style_checker():
    from pipeline.style.checker import StyleChecker
    return StyleChecker(config_path=PIPELINE_ROOT / "style" / "bible" / "style_config.json")


def _get_router():
    from pipeline.orchestrator.router import Router
    from pipeline.orchestrator.dispatcher import Dispatcher
    tracker = _get_tracker()
    dispatcher = Dispatcher(tools_dir=PIPELINE_ROOT / "orchestrator" / "tools")
    return Router(cost_tracker=tracker, dispatcher=dispatcher)


# ---------------------------------------------------------------------------
# generate
# ---------------------------------------------------------------------------

@app.command("generate")
def generate_cmd(
    type: str = typer.Option(..., "--type", "-t", help="Asset type: concept_art|3d_mesh|texture|character|environment|vfx|music|sfx|ui"),
    prompt: str = typer.Option(..., "--prompt", "-p", help="Generation prompt"),
    tags: str = typer.Option("", "--tags", help="Comma-separated tags"),
    force_paid: bool = typer.Option(False, "--force-paid", help="Skip free tiers, go straight to paid"),
    priority: str = typer.Option("normal", "--priority", help="low|normal|high"),
    name: str = typer.Option("", "--name", "-n", help="Asset name"),
):
    """Generate a single asset through the pipeline."""
    from pipeline.inventory.models import AssetRequest, Asset
    from pipeline.orchestrator.router import Router
    from pipeline.orchestrator.dispatcher import Dispatcher

    request = AssetRequest(
        id=str(uuid.uuid4()),
        type=type,
        prompt=prompt,
        style_refs=[],
        priority=priority,
        force_paid=force_paid,
        tags=[t.strip() for t in tags.split(",") if t.strip()],
    )

    tracker = _get_tracker()
    dispatcher = Dispatcher(tools_dir=PIPELINE_ROOT / "orchestrator" / "tools")
    router = Router(cost_tracker=tracker, dispatcher=dispatcher)

    console.print(f"\n[bold]Pipeline Generate[/bold]")
    console.print(f"  Type:     {request.type}")
    console.print(f"  Prompt:   {request.prompt}")
    console.print(f"  Priority: {request.priority}")
    if request.force_paid:
        console.print(f"  Mode:     [yellow]FORCE PAID[/yellow]")
    console.print()

    # Route
    decision = router.route(request)

    if decision.blocked:
        console.print(f"[bold red]✗ BLOCKED:[/bold red] {decision.block_reason}")
        raise typer.Exit(1)

    if decision.budget_warning:
        console.print(f"[yellow]⚠ {decision.budget_warning}[/yellow]")

    console.print(f"[bold green]→ Routing to:[/bold green] {decision.tool_name} ({decision.tier})")

    # Dispatch
    output_dir = PIPELINE_ROOT / "assets" / _type_to_dir(type)
    output_dir.mkdir(parents=True, exist_ok=True)

    result = dispatcher.dispatch(decision.tool_id, request, str(output_dir))

    if not result.success:
        console.print(f"[bold red]✗ Generation failed:[/bold red] {result.error}")
        raise typer.Exit(1)

    # Log cost
    if result.cost > 0:
        tracker.log_cost(decision.tool_id, result.cost, request.id)

    # Save to inventory
    inventory = _get_inventory()
    asset = Asset(
        id=request.id,
        type=type,
        name=name or _generate_name(type, request.tags),
        prompt=prompt,
        tool_used=decision.tool_id,
        tool_tier=decision.tier,
        cost_usd=result.cost,
        file_path=result.asset_path or "",
        style_check_result="pending",
        style_check_score=0.0,
        status="generated",
        tags=",".join(request.tags),
        created_at=datetime.now(timezone.utc).isoformat(),
        updated_at=datetime.now(timezone.utc).isoformat(),
        notes="",
    )
    inventory.insert(asset)

    # Style check
    checker = _get_style_checker()
    if result.asset_path and Path(result.asset_path).exists():
        style_result = checker.check(type, result.asset_path, result.metadata)
        inventory.update_style_check(request.id, style_result.result, style_result.score)
        if style_result.result == "fail":
            inventory.quarantine(request.id, "; ".join(style_result.issues))
            console.print(f"[red]✗ Style check FAILED — asset quarantined[/red]")
            for issue in style_result.issues:
                console.print(f"  - {issue}")
        elif style_result.result == "warn":
            console.print(f"[yellow]⚠ Style check WARNING (score: {style_result.score:.2f})[/yellow]")
            for issue in style_result.issues:
                console.print(f"  - {issue}")
        else:
            console.print(f"[green]✓ Style check passed (score: {style_result.score:.2f})[/green]")
    else:
        console.print("[dim]Style check skipped — no output file[/dim]")

    console.print(f"\n[bold green]✓ Asset generated:[/bold green] {request.id}")
    if result.asset_path:
        console.print(f"  File: {result.asset_path}")
    console.print(f"  Cost: ${result.cost:.2f}")


# ---------------------------------------------------------------------------
# run (batch)
# ---------------------------------------------------------------------------

@app.command("run")
def run_batch(
    request: Path = typer.Option(..., "--request", "-r", help="Path to batch request JSON file"),
):
    """Run a batch of asset generation requests from a JSON file."""
    if not request.exists():
        console.print(f"[red]File not found: {request}[/red]")
        raise typer.Exit(1)

    with open(request) as f:
        data = json.load(f)

    requests = data if isinstance(data, list) else data.get("requests", [data])
    console.print(f"\n[bold]Batch run: {len(requests)} request(s)[/bold]\n")

    for i, req_data in enumerate(requests, 1):
        console.print(f"[bold]── Request {i}/{len(requests)} ──[/bold]")
        try:
            generate_cmd.callback(
                type=req_data["type"],
                prompt=req_data["prompt"],
                tags=",".join(req_data.get("tags", [])),
                force_paid=req_data.get("force_paid", False),
                priority=req_data.get("priority", "normal"),
                name=req_data.get("name", ""),
            )
        except SystemExit:
            console.print(f"[yellow]Request {i} ended with non-zero exit[/yellow]")
        console.print()


# ---------------------------------------------------------------------------
# style
# ---------------------------------------------------------------------------

@style_app.command("check")
def style_check(
    asset_id: Optional[str] = typer.Argument(None, help="Asset ID to check"),
    file: Optional[Path] = typer.Option(None, "--file", "-f", help="File path to check directly"),
    type: str = typer.Option("concept_art", "--type", "-t", help="Asset type (used with --file)"),
):
    """Check style consistency for an asset or file."""
    checker = _get_style_checker()

    if file:
        if not file.exists():
            console.print(f"[red]File not found: {file}[/red]")
            raise typer.Exit(1)
        result = checker.check(type, str(file))
        _print_style_result(result)
        return

    if not asset_id:
        console.print("[red]Provide an asset_id or --file[/red]")
        raise typer.Exit(1)

    inventory = _get_inventory()
    asset = inventory.get(asset_id)
    if not asset:
        console.print(f"[red]Asset not found: {asset_id}[/red]")
        raise typer.Exit(1)

    if not asset.file_path or not Path(asset.file_path).exists():
        console.print(f"[yellow]No file on disk for asset {asset_id}[/yellow]")
        raise typer.Exit(1)

    result = checker.check(asset.type, asset.file_path)
    inventory.update_style_check(asset_id, result.result, result.score)

    if result.result == "fail":
        inventory.quarantine(asset_id, "; ".join(result.issues))

    _print_style_result(result)


@style_app.command("check-all")
def style_check_all():
    """Run style checks on all pending assets."""
    inventory = _get_inventory()
    checker = _get_style_checker()
    pending = inventory.list_assets(status="pending")
    # Filter to those with style_check_result == "pending"
    to_check = [a for a in pending if a.style_check_result == "pending"]

    if not to_check:
        console.print("[dim]No pending assets to check[/dim]")
        return

    console.print(f"\n[bold]Checking {len(to_check)} pending asset(s)...[/bold]\n")
    passed = warned = failed = skipped = 0

    for asset in to_check:
        if not asset.file_path or not Path(asset.file_path).exists():
            skipped += 1
            continue

        result = checker.check(asset.type, asset.file_path)
        inventory.update_style_check(asset.id, result.result, result.score)

        if result.result == "fail":
            inventory.quarantine(asset.id, "; ".join(result.issues))
            failed += 1
        elif result.result == "warn":
            warned += 1
        else:
            passed += 1

        status_icon = {"pass": "[green]✓[/green]", "warn": "[yellow]⚠[/yellow]", "fail": "[red]✗[/red]"}
        console.print(f"  {status_icon.get(result.result, '?')} {asset.id[:8]}... {result.result} ({result.score:.2f})")

    console.print(f"\n  Passed: {passed}  Warned: {warned}  Failed: {failed}  Skipped: {skipped}")


def _print_style_result(result):
    color = {"pass": "green", "warn": "yellow", "fail": "red"}.get(result.result, "white")
    console.print(f"\n[bold {color}]Style: {result.result.upper()}[/bold {color}] (score: {result.score:.2f})")
    if result.issues:
        for issue in result.issues:
            console.print(f"  - {issue}")


# ---------------------------------------------------------------------------
# inventory
# ---------------------------------------------------------------------------

@inventory_app.command("list")
def inventory_list(
    type: Optional[str] = typer.Option(None, "--type", "-t", help="Filter by asset type"),
    status: Optional[str] = typer.Option(None, "--status", "-s", help="Filter by status"),
    tag: Optional[str] = typer.Option(None, "--tag", help="Filter by tag"),
):
    """List assets in the inventory."""
    inventory = _get_inventory()
    assets = inventory.list_assets(type=type, status=status, tag=tag)

    if not assets:
        console.print("[dim]No assets found[/dim]")
        return

    table = Table(title="Asset Inventory")
    table.add_column("ID", style="dim", max_width=12)
    table.add_column("Type")
    table.add_column("Name")
    table.add_column("Tool")
    table.add_column("Tier")
    table.add_column("Cost", justify="right")
    table.add_column("Style", justify="center")
    table.add_column("Status")

    for a in assets:
        style_color = {"pass": "green", "warn": "yellow", "fail": "red", "pending": "dim"}.get(a.style_check_result or "", "white")
        status_color = {"approved": "green", "quarantined": "red", "generated": "blue", "integrated": "cyan", "archived": "dim"}.get(a.status or "", "white")

        table.add_row(
            a.id[:12] + "…" if len(a.id) > 12 else a.id,
            a.type,
            a.name or "",
            a.tool_used or "",
            a.tool_tier or "",
            f"${a.cost_usd:.2f}" if a.cost_usd else "$0.00",
            f"[{style_color}]{a.style_check_result or 'pending'}[/{style_color}]",
            f"[{status_color}]{a.status}[/{status_color}]",
        )

    console.print(table)


@inventory_app.command("show")
def inventory_show(
    asset_id: str = typer.Argument(..., help="Asset ID"),
):
    """Show details for a single asset."""
    inventory = _get_inventory()
    asset = inventory.get(asset_id)

    if not asset:
        console.print(f"[red]Asset not found: {asset_id}[/red]")
        raise typer.Exit(1)

    panel_content = Text()
    panel_content.append(f"ID:          {asset.id}\n")
    panel_content.append(f"Type:        {asset.type}\n")
    panel_content.append(f"Name:        {asset.name or '—'}\n")
    panel_content.append(f"Prompt:      {asset.prompt or '—'}\n")
    panel_content.append(f"Tool:        {asset.tool_used or '—'} ({asset.tool_tier or '—'})\n")
    panel_content.append(f"Cost:        ${asset.cost_usd:.2f}\n")
    panel_content.append(f"File:        {asset.file_path or '—'}\n")
    panel_content.append(f"Style:       {asset.style_check_result or 'pending'} ({asset.style_check_score or 0:.2f})\n")
    panel_content.append(f"Status:      {asset.status}\n")
    panel_content.append(f"Tags:        {asset.tags or '—'}\n")
    panel_content.append(f"Created:     {asset.created_at or '—'}\n")
    panel_content.append(f"Updated:     {asset.updated_at or '—'}\n")
    panel_content.append(f"Notes:       {asset.notes or '—'}")

    console.print(Panel(panel_content, title=f"Asset: {asset.id[:12]}…"))


@inventory_app.command("approve")
def inventory_approve(
    asset_id: str = typer.Argument(..., help="Asset ID to approve"),
):
    """Approve an asset for integration."""
    inventory = _get_inventory()
    asset = inventory.get(asset_id)
    if not asset:
        console.print(f"[red]Asset not found: {asset_id}[/red]")
        raise typer.Exit(1)
    inventory.approve(asset_id)
    console.print(f"[green]✓ Asset {asset_id[:12]}… approved[/green]")


@inventory_app.command("quarantine")
def inventory_quarantine(
    asset_id: str = typer.Argument(..., help="Asset ID to quarantine"),
    reason: str = typer.Option(..., "--reason", "-r", help="Reason for quarantine"),
):
    """Quarantine an asset for review."""
    inventory = _get_inventory()
    asset = inventory.get(asset_id)
    if not asset:
        console.print(f"[red]Asset not found: {asset_id}[/red]")
        raise typer.Exit(1)
    inventory.quarantine(asset_id, reason)
    console.print(f"[yellow]⚠ Asset {asset_id[:12]}… quarantined: {reason}[/yellow]")


@inventory_app.command("export")
def inventory_export(
    format: str = typer.Option("json", "--format", "-f", help="Export format: json|csv"),
):
    """Export all assets."""
    inventory = _get_inventory()
    output = inventory.export(format=format)
    console.print(output)


@inventory_app.command("stats")
def inventory_stats():
    """Show inventory statistics."""
    inventory = _get_inventory()
    stats = inventory.get_stats()

    console.print(f"\n[bold]Asset Inventory Statistics[/bold]\n")
    console.print(f"Total assets: [bold]{stats['total']}[/bold]")

    if stats.get("by_type"):
        type_str = " ".join(f"{k}({v})" for k, v in stats["by_type"].items())
        console.print(f"  By type: {type_str}")

    if stats.get("by_status"):
        status_str = " ".join(f"{k}({v})" for k, v in stats["by_status"].items())
        console.print(f"  By status: {status_str}")

    if stats.get("by_tier"):
        tier_str = " ".join(f"{k}({v})" for k, v in stats["by_tier"].items())
        console.print(f"  By tier: {tier_str}")

    console.print(f"Style pass rate: [bold]{stats.get('style_pass_rate', 0):.1f}%[/bold]")
    console.print(f"Total cost to date: [bold]${stats.get('total_cost', 0):.2f}[/bold]\n")


# ---------------------------------------------------------------------------
# costs
# ---------------------------------------------------------------------------

@costs_app.command("summary")
def costs_summary(
    month: Optional[str] = typer.Option(None, "--month", "-m", help="Month in YYYY-MM format"),
):
    """Show cost summary for a month."""
    tracker = _get_tracker()
    summary = tracker.get_summary(month=month)
    display_month = month or tracker._current_month()

    table = Table(title=f"Cost Summary — {display_month}")
    table.add_column("Tool")
    table.add_column("Tier")
    table.add_column("Spent", justify="right")
    table.add_column("Cap", justify="right")
    table.add_column("% Used", justify="right")
    table.add_column("Status", justify="center")

    for row in summary:
        pct = row["pct_used"]
        pct_str = f"{pct:.0f}%" if pct is not None else "—"
        cap_str = f"${row['cap']:.2f}" if row["cap"] else "—"

        status_map = {"ok": "[green]ok[/green]", "warning": "[yellow]⚠ near cap[/yellow]", "blocked": "[red]✗ blocked[/red]"}
        status = status_map.get(row["status"], row["status"])

        table.add_row(
            row["tool_name"],
            row["tier"],
            f"${row['spent']:.2f}",
            cap_str,
            pct_str,
            status,
        )

    console.print(table)

    total_spend = tracker.get_total_spend(month=month)
    config = tracker._load_config()
    total_budget = config["total_monthly_budget"]
    pct = (total_spend / total_budget * 100) if total_budget > 0 else 0
    console.print(f"\nTotal: [bold]${total_spend:.2f}[/bold] / ${total_budget:.2f} budget ({pct:.0f}%)\n")


@costs_app.command("by-tool")
def costs_by_tool(
    month: Optional[str] = typer.Option(None, "--month", "-m", help="Month in YYYY-MM format"),
):
    """Show detailed cost breakdown per tool."""
    tracker = _get_tracker()
    breakdown = tracker.get_by_tool(month=month)
    display_month = month or tracker._current_month()

    console.print(f"\n[bold]Cost Breakdown by Tool — {display_month}[/bold]\n")
    for entry in breakdown:
        if entry["spent"] > 0:
            console.print(f"  {entry['tool_name']}: ${entry['spent']:.2f} ({entry['calls']} calls)")

    if not any(e["spent"] > 0 for e in breakdown):
        console.print("  [dim]No spend recorded[/dim]")
    console.print()


@costs_app.command("forecast")
def costs_forecast():
    """Forecast end-of-month spend based on current rate."""
    tracker = _get_tracker()
    forecast = tracker.get_forecast()

    console.print(f"\n[bold]Spend Forecast[/bold]\n")
    console.print(f"  Current spend:     ${forecast['current_spend']:.2f}")
    console.print(f"  Days elapsed:      {forecast['days_elapsed']}")
    console.print(f"  Days in month:     {forecast['days_in_month']}")
    console.print(f"  Projected total:   [bold]${forecast['projected_total']:.2f}[/bold]")
    console.print(f"  Monthly budget:    ${forecast['monthly_budget']:.2f}")

    if forecast["projected_total"] > forecast["monthly_budget"]:
        over = forecast["projected_total"] - forecast["monthly_budget"]
        console.print(f"  [red]⚠ Projected to exceed budget by ${over:.2f}[/red]")
    else:
        remaining = forecast["monthly_budget"] - forecast["projected_total"]
        console.print(f"  [green]Projected headroom: ${remaining:.2f}[/green]")
    console.print()


@costs_app.command("set-cap")
def costs_set_cap(
    tool_id: str = typer.Argument(..., help="Tool ID"),
    amount: float = typer.Argument(..., help="New monthly cap in USD"),
):
    """Set a tool's monthly spending cap."""
    tracker = _get_tracker()
    try:
        tracker.set_cap(tool_id, amount)
        console.print(f"[green]✓ {tool_id} monthly cap set to ${amount:.2f}[/green]")
    except ValueError as e:
        console.print(f"[red]{e}[/red]")
        raise typer.Exit(1)


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def _type_to_dir(asset_type: str) -> str:
    mapping = {
        "concept_art": "concept_art",
        "3d_mesh": "meshes",
        "texture": "textures",
        "character": "meshes",
        "environment": "concept_art",
        "vfx": "concept_art",
        "music": "audio",
        "sfx": "audio",
        "ui": "concept_art",
    }
    return mapping.get(asset_type, "concept_art")


def _generate_name(asset_type: str, tags: list[str]) -> str:
    prefix_map = {
        "concept_art": "CA",
        "3d_mesh": "SM",
        "texture": "T",
        "character": "CH",
        "environment": "ENV",
        "vfx": "VFX",
        "music": "MUS",
        "sfx": "SFX",
        "ui": "UI",
    }
    prefix = prefix_map.get(asset_type, "ASSET")
    tag_part = "_".join(tags[:3]) if tags else "unnamed"
    return f"{prefix}_{tag_part}"


# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------

def main():
    try:
        from dotenv import load_dotenv
        load_dotenv(PIPELINE_ROOT / ".env")
    except ImportError:
        pass
    app()


if __name__ == "__main__":
    main()
