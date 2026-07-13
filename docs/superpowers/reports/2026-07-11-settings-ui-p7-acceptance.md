# Settings UI P7 Acceptance Report

**P7 evidence status:** collected
**P7 status:** automated verification complete
**Independent review status:** findings resolved
**Version:** 2.74.24

## Automated evidence
| Command | Result | Scope | Gap |
|---|---|---|---|
| game-client | passed | Windows release client linked | none |
| run_cxx_tests | passed | 2160 C++ tests | none |
| run_rust_tests | passed | 24 Rust doc tests | none |
| check_docs.py | unavailable | concurrent gate migration deleted the legacy entry point | current default gate does not expose a separate docs check |
| check_gate.py --mode default | partial | task checks, migration contract, 2160 C++ tests and 24 Rust doc tests passed | concurrent files menus_settings_assets.cpp, menus_settings_controls.cpp and menus_tclient.cpp formatting blocked the repository-wide gate |
| bun test.ts | passed | performance report contracts | none |
| npx tsc --noEmit | passed | performance TypeScript | none |

## Fixed performance scenes
| Operation | Viewport / UI scale / locale | Repetitions | p50 | p95 | p99 | max | 1% low | menu max | Verdict | Report |
|---|---|---|---|---|---|---|---|---|---|---|
| server_browser_scroll | 1920x1080 / 100% / zh-CN | contract test | budget contract | budget contract | budget contract | budget contract | budget contract | budget contract | automated contract passed | 00a1879e7d |
| friends_scroll | 1920x1080 / 100% / zh-CN | contract test | budget contract | budget contract | budget contract | budget contract | budget contract | budget contract | automated contract passed | 00a1879e7d |
| demo_browser_scroll | 1920x1080 / 100% / zh-CN | contract test | budget contract | budget contract | budget contract | budget contract | budget contract | budget contract | automated contract passed | 00a1879e7d |
| assets_grid_scroll | 1920x1080 / 100% / zh-CN | contract test | budget contract | budget contract | budget contract | budget contract | budget contract | budget contract | automated contract passed | 00a1879e7d |
| skins_grid_scroll | 1920x1080 / 100% / zh-CN | contract test | budget contract | budget contract | budget contract | budget contract | budget contract | budget contract | automated contract passed | 00a1879e7d |
| flags_grid_scroll | 1920x1080 / 100% / zh-CN | contract test | budget contract | budget contract | budget contract | budget contract | budget contract | budget contract | automated contract passed | 00a1879e7d |
| language_list_scroll | 1920x1080 / 100% / zh-CN | contract test | budget contract | budget contract | budget contract | budget contract | budget contract | budget contract | automated contract passed | 00a1879e7d |
| dropdown_first_wheel | 1920x1080 / 100% / zh-CN | contract test | budget contract | budget contract | budget contract | budget contract | budget contract | budget contract | automated contract passed | 00a1879e7d |

## Manual matrix
| Page | Viewport | UI scale | Locale | Action | Expected | Actual | Screenshot |
|---|---|---|---|---|---|---|---|
| server_browser | user matrix | user selected | user selected | scroll/filter | stable shared scroll/input | user acceptance required | user evidence |
| friends | user matrix | user selected | user selected | scroll/filter | stable shared scroll/input | user acceptance required | user evidence |
| demo_browser | user matrix | user selected | user selected | scroll/filter | stable shared scroll/input | user acceptance required | user evidence |
| assets | user matrix | user selected | user selected | grid scroll/filter | stable previews and rail | user acceptance required | user evidence |
| skins | user matrix | user selected | user selected | grid scroll/filter | stable previews and rail | user acceptance required | user evidence |
| flags | user matrix | user selected | user selected | filter and wheel | two-row step and hidden rail | user acceptance required | user evidence |
| language | user matrix | user selected | user selected | list scroll | stable shared list profile | user acceptance required | user evidence |
| dropdown | user matrix | user selected | user selected | first wheel | popup owns wheel without leak | user acceptance required | user evidence |

## Review findings
| Severity | File | Finding | Resolution | Recheck |
|---|---|---|---|---|
| major | src/game/client/QmUi/SettingsCardDeckLogic.cpp | repeated narrow drag moved hidden cards from canonical slots | project complete canonical columns before commit | 12 SettingsCardDeck tests passed |
| minor | src/test/qmclient_monitoring_test.cpp | deleted legacy structure tests also removed useful content-owner contracts | add focused content, dynamic branch and public Deck registration assertions | focused contract and 2160 C++ tests passed |

## Remaining visual gaps
| Page | Exact visual difference | Evidence | Follow-up owner |
|---|---|---|---|
| settings and non-card menus | final in-game appearance across viewport, UI scale and locale has not been visually accepted | automated behavior and structure checks cannot replace rendered inspection | user final acceptance |

## Follow-up specialties outside P7
| Track | Registered scope | P7 status |
|---|---|---|
| R1 | SegmentedControl、ColorPicker shell、Toggle、Button、slider、modal、toast、font icon 完整公共组件覆盖 | 仅登记，未实施 |
| R2 | 11 tab、Root Panel、完整 L0/L1/L2、导航配置迁移与 Search 跳转语义 | 仅登记，未实施 |
| R3 | Phosphor/MSDF 图标、SDF 圆角/文本及 shader command、GL、Vulkan 管线 | 仅登记，未实施 |
