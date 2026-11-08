"""Source regression for opaque interior-mirror rim depth coverage."""
from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[1]


class MirrorDepthTests(unittest.TestCase):
    def test_mapped_glass_stream_is_reused_and_invalidated(self):
        proxy = (ROOT / "src" / "d3d9proxy.cpp").read_text()
        mapping = proxy.split("static void MapGlassVertices(", 1)[1].split(
            "// The glass's draw", 1
        )[0]
        self.assertIn("cached.source == vb", mapping)
        self.assertIn("SetStreamSource(0, cached.stream", mapping)
        self.assertIn("cached.valid = true", mapping)
        release_targets = proxy.split("static void ReleaseEyeTargets() {", 1)[1].split("\n}\n", 1)[0]
        self.assertIn("ReleaseMirrorMappedStreams();", release_targets)
        hooks = (ROOT / "src" / "driver_buffer_hooks.h").read_text()
        self.assertIn("InvalidateMirrorMappedStreams(vb);", hooks)
        process_vertices = proxy.split("HRESULT STDMETHODCALLTYPE ProcessVertices", 1)[1].split("}", 1)[0]
        self.assertIn("InvalidateMirrorMappedStreams(vb);", process_vertices)

    def test_skipped_rear_view_keeps_image_without_recopying_it(self):
        source = (ROOT / "src" / "game.cpp").read_text()
        end_mirrors = source.split("void EndMirrors() {", 1)[1].split("}", 1)[0]
        self.assertIn("s_viewSkipped[kRearView] ? nullptr : RvmSurface()", end_mirrors)
        proxy = (ROOT / "src" / "d3d9proxy.cpp").read_text()
        end_mirror_eye = proxy.split("void D3D_EndMirrorEye(void* rearViewSurface) {", 1)[1]
        end_mirror_eye = end_mirror_eye.split("// --- the in-car dashboard", 1)[0]
        self.assertIn("if (rearViewSurface) MirrorTexture((IDirect3DSurface9*)rearViewSurface);", end_mirror_eye)

    def test_rim_writes_depth_in_model_pass(self):
        source = (ROOT / "src" / "d3d9proxy.cpp").read_text()
        draw = source.split("static void DrawMirrorGlass(const PendingMirror& m, bool depth) {", 1)[1]
        draw = draw.split("// --- the steering wheel", 1)[0]
        rim = draw.split("if (!m.frame.empty()) {", 1)[1]
        rim = rim.split("s_dev->DrawPrimitiveUP", 1)[0]
        self.assertIn("SetRenderState(D3DRS_ZWRITEENABLE, depth ? TRUE : FALSE)", rim)
        self.assertNotIn("SetRenderState(D3DRS_ZWRITEENABLE, FALSE)", rim)
        self.assertIn("saved->Apply();", draw)

    def test_picture_is_closer_than_rim(self):
        source = (ROOT / "src" / "cockpit.cpp").read_text()
        self.assertIn("GlassVertices(glass, mat, frame, 0.006f + g_cfg.mirrorOffset", source)
        self.assertIn("GlassVertices(glass, mat, verts, 0.010f + g_cfg.mirrorOffset", source)

    def test_fallback_remains_depth_free(self):
        source = (ROOT / "src" / "d3d9proxy.cpp").read_text()
        self.assertIn("DrawMirrorGlass(m, true);", source)
        self.assertIn("DrawMirrorGlass(m, false);", source)
        self.assertIn("SetRenderState(D3DRS_ZENABLE, depth ? D3DZB_TRUE : D3DZB_FALSE)", source)


if __name__ == "__main__":
    unittest.main()