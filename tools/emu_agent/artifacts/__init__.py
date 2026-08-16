"""Reference-only artifact registry for emu-agent."""

from .references import ArtifactError, ArtifactRegistry, bounded_slice, reference

__all__ = ["ArtifactError", "ArtifactRegistry", "bounded_slice", "reference"]
