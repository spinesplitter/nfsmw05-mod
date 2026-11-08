"""Source regression for opaque interior-mirror rim depth coverage."""
from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[1]


class MirrorDepthTests(unittest.TestCase):
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