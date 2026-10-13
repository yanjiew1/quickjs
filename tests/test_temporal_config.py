import importlib.util
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("feature_config", ROOT / "tools/ci/test262-config.py")
config = importlib.util.module_from_spec(spec)
spec.loader.exec_module(config)

class TemporalConfig(unittest.TestCase):
    def test_preserve_unrelated_policy_and_enable_selected_frontend(self):
        source = ("[config]\nreportfile=custom.txt\n[features]\n"
                  "Temporal=skip\nIntl.Era-monthcode=skip\nIntl.Locale=skip\n"
                  "UnknownFeature=skip\n[exclude]\nslow-test.js\n")
        active = config.derive(source, "n", "y", preserve_reportfile=True)
        self.assertIn("\nTemporal\n", active)
        self.assertIn("\nIntl.Era-monthcode\n", active)
        self.assertIn("\nIntl.Locale=skip\n", active)
        self.assertIn("UnknownFeature=skip\n", active)
        self.assertIn("reportfile=custom.txt\n", active)
        self.assertTrue(active.endswith("[exclude]\nslow-test.js\n"))
        inactive = config.derive(source, "n", "n")
        self.assertIn("Temporal=skip\n", inactive)
        self.assertIn("Intl.Era-monthcode=skip\n", inactive)
    def test_real_config_and_distinct_hosted_report(self):
        source = (ROOT / "test262.conf").read_text()
        active = config.derive(source, "n", "y")
        self.assertIn("\nTemporal\n", active)
        self.assertIn("reportfile=test262_temporal_report.txt\n", active)
        self.assertIn("test262/test/intl402/\n", active)

if __name__ == "__main__":
    unittest.main()
