import fs from "node:fs/promises";
import path from "node:path";
import { pathToFileURL } from "node:url";
import { FileBlob, PresentationFile } from "@oai/artifact-tool";

const workspaceDir = "/Users/saitougenbu/Desktop/ssd_mobilenet_v2_fpnlite_320x320_coco17_tpu-8/Sasken_AIML_Project";
const skillDir = "/Users/saitougenbu/.codex/plugins/cache/openai-primary-runtime/presentations/26.904.11930/skills/presentations";
const referencePath = "/Users/saitougenbu/.codex/plugins/cache/openai-curated-remote/openai-templates/0.1.1/skills/artifact-template-project-kickoff/assets/reference.pptx";
const assetsDir = path.join(workspaceDir, ".codex-build/fcw-slide-examples/assets");
const previewDir = path.join(workspaceDir, ".codex-build/fcw-slide-examples/previews");
const stagingDir = path.join(workspaceDir, ".codex-finalizer");
const finalPath = path.join(workspaceDir, "deliverables/FCW_7_slide_examples_2026-09-06_v2.pptx");
const runtimePython = "/Users/saitougenbu/.cache/codex-runtimes/codex-primary-runtime/dependencies/python/bin/python3";
process.env.RUNTIME_NODE_MODULES = "/Users/saitougenbu/.cache/codex-runtimes/codex-primary-runtime/dependencies/node/node_modules";

const { finalizePresentation } = await import(
  pathToFileURL(path.join(skillDir, "container_tools/artifact_tool_utils.mjs")).href,
);

const COLORS = {
  background: "#002B20",
  lime: "#B9F64A",
  white: "#F5F8F4",
  card: "#2D5143",
  muted: "#B8C9C1",
  darkText: "#06281F",
};
const JP_FONT = "Hiragino Sans";
const LATIN_FONT = "Inter";

await fs.mkdir(previewDir, { recursive: true });
await fs.mkdir(stagingDir, { recursive: true });
await fs.mkdir(path.dirname(finalPath), { recursive: true });

const presentation = await PresentationFile.importPptx(await FileBlob.load(referencePath));
const originals = [...presentation.slides.items];

const slides = [
  originals[2].duplicate(),
  originals[3].duplicate(),
  originals[5].duplicate(),
  originals[8].duplicate(),
  originals[9].duplicate(),
  originals[7].duplicate(),
  originals[2].duplicate(),
];

for (const slide of originals) slide.delete();
for (let index = 0; index < slides.length; index += 1) slides[index].moveTo(index);

function textOf(shape) {
  return shape?.text?.toString?.() ?? "";
}

function placeholderType(shape) {
  return shape?.placeholder?.type;
}

function findTitle(slide) {
  return slide.shapes.items.find((shape) => placeholderType(shape) === "title");
}

function setTitle(slide, text) {
  const title = findTitle(slide);
  if (!title) throw new Error(`Title placeholder not found on slide ${slide.index + 1}`);
  title.text = text;
  title.text.style = {
    typeface: JP_FONT,
    fontSize: 40,
    bold: false,
    color: COLORS.white,
    autoFit: "shrinkText",
    lineSpacing: 1.0,
  };
}

function setSlideNumber(slide, number) {
  const shape = slide.shapes.items.find((item) => placeholderType(item) === "slideNumber");
  if (!shape) return;
  shape.text = String(number);
  shape.text.style = {
    typeface: LATIN_FONT,
    fontSize: 13,
    color: COLORS.white,
    alignment: "right",
  };
}

function setTwoPartBlock(shape, heading, body, options = {}) {
  const headingSize = options.headingSize ?? "22pt";
  const bodySize = options.bodySize ?? "16pt";
  shape.text.set([
    {
      spaceAfter: 700,
      runs: [{
        run: heading,
        textStyle: {
          typeface: JP_FONT,
          fontSize: headingSize,
          bold: true,
          color: options.headingColor ?? COLORS.lime,
        },
      }],
    },
    {
      runs: [{
        run: body,
        textStyle: {
          typeface: JP_FONT,
          fontSize: bodySize,
          color: options.bodyColor ?? COLORS.white,
        },
      }],
    },
  ]);
  shape.text.style = {
    typeface: JP_FONT,
    autoFit: "shrinkText",
    verticalAlignment: "top",
    lineSpacing: 1.08,
    insets: { top: 4, right: 4, bottom: 4, left: 0 },
  };
}

function setPlainText(shape, text, options = {}) {
  shape.text = text;
  shape.text.style = {
    typeface: options.typeface ?? JP_FONT,
    fontSize: options.fontSize ?? 22,
    bold: options.bold ?? false,
    color: options.color ?? COLORS.white,
    alignment: options.alignment ?? "left",
    verticalAlignment: options.verticalAlignment ?? "top",
    autoFit: options.autoFit ?? "shrinkText",
    lineSpacing: options.lineSpacing ?? 1.08,
    insets: options.insets ?? { top: 2, right: 2, bottom: 2, left: 2 },
  };
}

function addTextBox(slide, text, position, options = {}) {
  const box = slide.shapes.add({
    geometry: options.geometry ?? "textbox",
    name: options.name,
    position,
    fill: options.fill ?? "none",
    line: options.line ?? { style: "solid", fill: "none", width: 0 },
    borderRadius: options.borderRadius,
  });
  setPlainText(box, text, options);
  return box;
}

function setNotes(slide, script, sources = []) {
  const sourceText = sources.length
    ? `\n\nSources (not spoken):\n${sources.map((source) => `- ${source}`).join("\n")}`
    : "";
  slide.speakerNotes.textFrame.setText(`${script}${sourceText}`);
  slide.speakerNotes.setVisible(true);
}

function replaceFirstImage(slide, imagePath, alt, frame) {
  const image = slide.images.items[0];
  if (!image) throw new Error(`Image placeholder not found on slide ${slide.index + 1}`);
  return fs.readFile(imagePath).then((bytes) => {
    image.replace({ blob: bytes, contentType: "image/png", alt, fit: "contain" });
    image.frame = frame;
    image.geometry = "roundRect";
    image.borderRadius = "rounded-xl";
    image.lockAspectRatio = false;
    return image;
  });
}

// Slide 1: Python prototype role and real output frame.
{
  const slide = slides[0];
  setTitle(slide, "Python-based FCW Prototype on Mac");
  const blocks = slide.shapes.items
    .filter((shape) => textOf(shape).startsWith("Title here"))
    .sort((a, b) => a.frame.top - b.frame.top);
  setTwoPartBlock(
    blocks[0],
    "目的",
    "録画映像を使い、FCWロジックの基準動作を実機移植前に確認",
  );
  setTwoPartBlock(
    blocks[1],
    "入出力",
    "H.264映像を入力し、結果動画、CSVログ、警告音を出力",
  );
  await replaceFirstImage(
    slide,
    path.join(assetsDir, "python-flow-frame.png"),
    "Mac上のPython FCWプロトタイプによる車両検出と前方ROIの表示例",
    { left: 658, top: 154, width: 581, height: 327 },
  );
  addTextBox(slide, "RECORDED VIDEO INPUT", { left: 658, top: 108, width: 226, height: 34 }, {
    geometry: "roundRect",
    fill: COLORS.lime,
    borderRadius: "rounded-full",
    fontSize: 15,
    bold: true,
    color: COLORS.darkText,
    alignment: "center",
    verticalAlignment: "middle",
  });
  addTextBox(slide, "実際の出力画面：ROI、検出枠、track ID", { left: 658, top: 500, width: 581, height: 42 }, {
    fontSize: 18,
    color: COLORS.muted,
    alignment: "center",
  });
  setNotes(
    slide,
    "まず、Mac上で実装したPython-based FCW Prototypeについて説明します。このプロトタイプの目的は、実機へ移植する前に、車両検出から警報判定までのFCWロジックを一つの処理として確認することです。入力には録画映像を使用し、フレームごとに処理します。結果は映像上に表示し、処理後の動画とCSVログとして保存します。Macでは警報音も出力します。このPython版を、後のTDA4VM実装で参照する基準動作として使用しました。",
    [
      "code/ssd_ttc.py",
      "code/fcw/config.py",
      "Image: output/ssd_ttc_result_0.4_1788427235.889101.mp4 at 28 s",
    ],
  );
}

// Slide 2: detection, ROI, tracking.
{
  const slide = slides[1];
  setTitle(slide, "Detection / ROI / Tracking");
  const callouts = slide.shapes.items
    .filter((shape) => textOf(shape).startsWith("Title here"))
    .sort((a, b) => a.frame.left - b.frame.left);
  const labels = slide.shapes.items
    .filter((shape) => textOf(shape) === "Supertitle")
    .sort((a, b) => a.frame.left - b.frame.left);
  const labelText = ["DETECTION", "FORWARD ROI", "TRACKING"];
  const headingText = [
    "SSD MobileNet V2",
    "前方車両を選別",
    "IoUでIDを維持",
  ];
  const bodyText = [
    "COCOのcarクラスのみを対象\nscore 0.4以上を採用",
    "bboxの下辺中心を判定\nROI外は後段から除外",
    "IoU threshold 0.3\n同じ車両の高さ履歴を保持",
  ];
  for (let index = 0; index < 3; index += 1) {
    setPlainText(labels[index], labelText[index], {
      typeface: LATIN_FONT,
      fontSize: 18,
      bold: true,
      color: COLORS.lime,
    });
    setTwoPartBlock(callouts[index], headingText[index], bodyText[index], {
      headingColor: COLORS.white,
      headingSize: "19pt",
      bodySize: "15pt",
    });
  }
  for (const left of [420, 831]) {
    slide.shapes.add({
      geometry: "rightArrow",
      position: { left, top: 376, width: 24, height: 28 },
      fill: COLORS.lime,
      line: { style: "solid", fill: "none", width: 0 },
    });
  }
  setNotes(
    slide,
    "Python版では、まずSSD MobileNet V2を使って各フレームの車両を検出します。COCOデータセットの車クラスだけを対象にし、信頼度が0.4以上の検出を残します。次に、画像上に設定した台形ROIを使い、バウンディングボックスの下辺中心がROI内にある車両だけを選別します。これにより、自車の前方とは考えにくい検出を後段から除外します。その後、前フレームとのIoUを使って車両IDを引き継ぎます。同じIDを維持することで、同じ車両の高さ変化を追跡できます。",
    [
      "code/fcw/detector.py",
      "code/fcw/roi.py",
      "code/fcw/tracker.py",
      "code/fcw/config.py",
    ],
  );
}

// Slide 3: TTC and alert decision.
{
  const slide = slides[2];
  setTitle(slide, "TTC Estimation and Alert Decision");
  const textShapes = slide.shapes.items.filter((shape) => textOf(shape).length > 0);
  const leftHeading = textShapes.find((shape) => textOf(shape).startsWith("Lorem ipsum dolor sit amet, \n"));
  const leftBody = textShapes.find((shape) => textOf(shape).startsWith("Lorem ipsum dolor sit amet, consectetur"));
  const cards = textShapes
    .filter((shape) => textOf(shape).startsWith("Title goes here"))
    .sort((a, b) => a.frame.top - b.frame.top);
  setPlainText(leftHeading, "車両高さ h(t) の履歴", {
    fontSize: 30,
    color: COLORS.white,
    lineSpacing: 1.05,
  });
  leftBody.text.set([
    { runs: [{ run: "1", textStyle: { typeface: LATIN_FONT, fontSize: "20pt", bold: true, color: COLORS.lime } }, { run: "   5フレームを保持", textStyle: { typeface: JP_FONT, fontSize: "18pt", color: COLORS.white } }] },
    { runs: [{ run: "2", textStyle: { typeface: LATIN_FONT, fontSize: "20pt", bold: true, color: COLORS.lime } }, { run: "   線形回帰で dh/dt を推定", textStyle: { typeface: JP_FONT, fontSize: "18pt", color: COLORS.white } }] },
    { runs: [{ run: "3", textStyle: { typeface: LATIN_FONT, fontSize: "20pt", bold: true, color: COLORS.lime } }, { run: "   TTC = h / (dh/dt)", textStyle: { typeface: JP_FONT, fontSize: "18pt", color: COLORS.white } }] },
  ]);
  leftBody.text.style = {
    typeface: JP_FONT,
    autoFit: "shrinkText",
    lineSpacing: 1.3,
    color: COLORS.white,
    insets: { top: 0, right: 0, bottom: 0, left: 0 },
  };
  setTwoPartBlock(cards[0], "警報ON", "TTC 4.0秒以下が\n3フレーム連続", {
    headingColor: COLORS.lime,
    headingSize: "20pt",
    bodySize: "17pt",
  });
  setTwoPartBlock(cards[1], "警報OFF", "TTC 6.5秒以上\nまたはTTCなし", {
    headingColor: COLORS.lime,
    headingSize: "20pt",
    bodySize: "17pt",
  });
  addTextBox(slide, "dh/dt 0以下：非接近としてTTCなし", { left: 41, top: 556, width: 500, height: 48 }, {
    geometry: "roundRect",
    fill: COLORS.card,
    borderRadius: "rounded-full",
    fontSize: 18,
    color: COLORS.lime,
    alignment: "center",
    verticalAlignment: "middle",
  });
  setNotes(
    slide,
    "追跡した車両ごとに、バウンディングボックスの高さを5フレーム分保持します。車両が接近すると画像上の高さが増えるため、高さと時間の関係を線形回帰し、dh/dtを求めます。現在の高さをdh/dtで割ることでTTCを推定します。高さが増加していない場合は接近していないと判断し、TTCを計算しません。現行設定では、TTCが4秒以下の状態が3フレーム連続すると警報を開始し、6.5秒以上になると解除します。異なる開始条件と解除条件によって、警報の頻繁な切り替わりを抑えます。",
    ["code/fcw/ttc.py", "code/fcw/alert.py", "code/fcw/config.py"],
  );
}

// Slide 4: output surfaces.
{
  const slide = slides[3];
  setTitle(slide, "Prototype Outputs");
  const topic = slide.shapes.items.find((shape) => textOf(shape).startsWith("Topic\n"));
  setTwoPartBlock(
    topic,
    "確認方法",
    "画面表示と保存データを突き合わせ、警報判定の過程を確認\n位置づけ：実機移植前のロジック検証用",
    { headingColor: COLORS.white, headingSize: "18pt", bodySize: "15pt" },
  );
  const values = slide.shapes.items
    .filter((shape) => ["12%", "34%", "56%"].includes(textOf(shape)))
    .sort((a, b) => a.frame.left - b.frame.left);
  const details = slide.shapes.items
    .filter((shape) => textOf(shape).startsWith("Lorem ipsum dolor sit amet"))
    .sort((a, b) => a.frame.left - b.frame.left);
  const valueText = ["VIDEO", "CSV", "AUDIO"];
  const detailText = [
    "検出枠、ID、TTC、R²、ALERTを重ねて表示",
    "frame、height、dh/dt、TTC、score、IoUを保存",
    "警報中にmacOSのシステム音を再生",
  ];
  for (let index = 0; index < 3; index += 1) {
    setPlainText(values[index], valueText[index], {
      typeface: LATIN_FONT,
      fontSize: 48,
      bold: false,
      color: COLORS.lime,
      verticalAlignment: "middle",
    });
    setPlainText(details[index], detailText[index], {
      fontSize: 20,
      color: COLORS.white,
      lineSpacing: 1.12,
    });
  }
  setNotes(
    slide,
    "判定結果は、映像上のバウンディングボックスやラベルとして確認できます。通常の車両は緑枠、警報中の車両は赤枠で表示し、ID、検出信頼度、IoU、履歴長、TTC、警報状態を重ねます。また、Macでは警報時にシステム音を再生し、処理後のMP4動画とCSVログを保存します。CSVでは、車両高さ、dh/dt、TTC、決定係数、警報状態を数値で確認できます。一方で、この値は画像上の高さから求めた推定値です。Python版で基準動作を確認した後、実機への移植に進みました。",
    [
      "code/fcw/visualizer.py",
      "code/fcw/logger.py",
      "code/fcw/alarm.py",
      "code/ssd_ttc.py",
    ],
  );
}

// Slide 5: seven-step development flow.
{
  const slide = slides[4];
  setTitle(slide, "Development Flow");
  for (const table of [...slide.tables.items]) slide.tables.deleteById(table.id);
  for (const shape of [...slide.shapes.items]) {
    const type = placeholderType(shape);
    if (!["title", "footer", "slideNumber"].includes(type)) shape.delete();
  }
  addTextBox(slide, "Macで確認した基準動作を、映像入力から統合まで順に実機へ移行", { left: 42, top: 122, width: 1196, height: 52 }, {
    fontSize: 23,
    color: COLORS.muted,
  });
  slide.shapes.add({
    geometry: "line",
    position: { left: 98, top: 328, width: 1082, height: 0 },
    fill: "none",
    line: { style: "solid", fill: COLORS.lime, width: 3 },
  });
  const stages = [
    "Mac\n(Python)",
    "TDA4VM\nSetup",
    "AVP2\nExecution",
    "H.264\nDecode",
    "3ch to 1ch",
    "OD / TTC\nIntegration",
    "Demo /\nResult",
  ];
  const centers = [100, 280, 460, 640, 820, 1000, 1180];
  for (let index = 0; index < stages.length; index += 1) {
    const center = centers[index];
    const circle = slide.shapes.add({
      geometry: "ellipse",
      position: { left: center - 18, top: 310, width: 36, height: 36 },
      fill: COLORS.lime,
      line: { style: "solid", fill: COLORS.lime, width: 1 },
    });
    setPlainText(circle, String(index + 1), {
      typeface: LATIN_FONT,
      fontSize: 17,
      bold: true,
      color: COLORS.darkText,
      alignment: "center",
      verticalAlignment: "middle",
      insets: { top: 0, right: 0, bottom: 0, left: 0 },
    });
    addTextBox(slide, stages[index], {
      left: center - 72,
      top: index % 2 === 0 ? 214 : 372,
      width: 144,
      height: 70,
    }, {
      fontSize: 20,
      bold: index === 0 || index === stages.length - 1,
      color: index === 0 || index === stages.length - 1 ? COLORS.lime : COLORS.white,
      alignment: "center",
      verticalAlignment: "middle",
      lineSpacing: 1.0,
    });
  }
  addTextBox(slide, "各段階で出力を確認してから次の処理を接続", { left: 300, top: 555, width: 680, height: 52 }, {
    geometry: "roundRect",
    fill: COLORS.card,
    borderRadius: "rounded-full",
    fontSize: 20,
    color: COLORS.white,
    alignment: "center",
    verticalAlignment: "middle",
  });
  setNotes(
    slide,
    "ここからはDevelopment Flowについて説明します。開発は、Mac上でPython版のFCWロジックを確認するところから始めました。その後、TDA4VMのセットアップとAVP2の実行へ進みました。映像処理では、H.264 decodingと3chから1chへの変換を順番に確認しました。映像が後段へ安定して届く状態を作ってから、Object DetectionとTTCを統合しました。最後にDemoとResultで一連の動作を確認します。段階ごとに出力を確認したことで、問題が発生した処理を切り分けやすくしました。",
  );
}

// Slide 6: input pipeline milestones.
{
  const slide = slides[5];
  setTitle(slide, "Video Input Pipeline on TDA4VM");
  const timeline = slide.shapes.items.find(
    (shape) => shape.name === "Google Shape;2259;p159",
  );
  if (timeline) {
    timeline.position = { left: 41, top: 354.2, width: 1198, height: 0.03 };
  }
  const stepLabels = slide.shapes.items
    .filter((shape) => textOf(shape) === "Date")
    .sort((a, b) => a.frame.left - b.frame.left);
  const milestones = slide.shapes.items
    .filter((shape) => textOf(shape).startsWith("Title here"))
    .sort((a, b) => a.frame.left - b.frame.left);
  const stepText = ["STEP 1", "STEP 2", "STEP 3"];
  const titles = ["TDA4VM Setup", "AVP2 Execution", "H.264 Decode / 3ch to 1ch"];
  const bodies = [
    "実行環境を準備し、ビルド・配置・起動を確認",
    "ベースパイプラインを起動し、処理グラフを確認",
    "フレーム取得と入力形式の変換を確認",
  ];
  for (let index = 0; index < 3; index += 1) {
    setPlainText(stepLabels[index], stepText[index], {
      typeface: LATIN_FONT,
      fontSize: 17,
      bold: true,
      color: COLORS.lime,
    });
    setTwoPartBlock(milestones[index], titles[index], bodies[index], {
      headingColor: COLORS.white,
      headingSize: "18pt",
      bodySize: "14pt",
    });
  }
  setNotes(
    slide,
    "実機側の最初の段階では、TDA4VM上で必要な環境を準備し、ビルド、配置、起動ができることを確認しました。次にAVP2のベースパイプラインを実行し、処理グラフが動作する状態を作りました。その後、入力処理にH.264 decodingを接続し、映像からフレームを取得できることを確認しました。この時点ではTTCロジックを追加せず、映像が継続的に流れることを優先して確認しました。続いて3chから1chへの変換を接続し、後段が受け取るデータ形式を整えました。これにより、次の統合段階へ進める入力経路が完成しました。",
  );
}

// Slide 7: final integration and demo placeholder.
{
  const slide = slides[6];
  setTitle(slide, "Object Detection / TTC Integration");
  const blocks = slide.shapes.items
    .filter((shape) => textOf(shape).startsWith("Title here"))
    .sort((a, b) => a.frame.top - b.frame.top);
  setTwoPartBlock(
    blocks[0],
    "統合",
    "Detection出力をROI、Tracking、TTC、Alertへ接続",
  );
  setTwoPartBlock(
    blocks[1],
    "Demo / Result",
    "検出枠、TTC、警報表示を確認し、Mac版の基準動作と比較",
  );
  await replaceFirstImage(
    slide,
    path.join(assetsDir, "python-alert-frame-2.png"),
    "Mac版FCWプロトタイプの警報表示。TTC 1.73秒で赤いALERT表示が出ている",
    { left: 658, top: 142, width: 581, height: 327 },
  );
  addTextBox(slide, "MAC REFERENCE OUTPUT", { left: 658, top: 100, width: 252, height: 32 }, {
    geometry: "roundRect",
    fill: COLORS.lime,
    borderRadius: "rounded-full",
    fontSize: 14,
    bold: true,
    color: COLORS.darkText,
    alignment: "center",
    verticalAlignment: "middle",
  });
  addTextBox(slide, "TDA4VM Demo\n最終版では実機キャプチャを配置し、同じ表示項目で比較", { left: 658, top: 492, width: 581, height: 110 }, {
    geometry: "roundRect",
    fill: COLORS.card,
    borderRadius: "rounded-xl",
    fontSize: 20,
    color: COLORS.white,
    alignment: "center",
    verticalAlignment: "middle",
    lineSpacing: 1.12,
  });
  setNotes(
    slide,
    "最後に、実機で得られたObject Detectionの結果を、FCWの後段処理へ接続します。検出結果を受けて前方車両を選別し、車両を追跡しながらTTCを計算して警報状態を更新します。Mac版で確認した処理の流れを基準にすることで、実機側の結果を段階ごとに比較できます。統合後は、映像上の検出枠、車両ID、TTC、警報表示をDemoとResultとして確認します。このスライドの右上はMac版の基準出力です。最終発表では、右下の枠をTDA4VM実機のキャプチャへ差し替え、両者を比較して締めます。",
    [
      "0904_new/codeC/main_pre.c",
      "0904_new/app_tidl_avp2/main.c",
      "Image: output/ssd_ttc_result_0.4_1788427235.889101.mp4 at 114.28 s",
    ],
  );
}

for (let index = 0; index < slides.length; index += 1) setSlideNumber(slides[index], index + 1);

if (presentation.slides.items.length !== 7) {
  throw new Error(`Expected 7 slides, found ${presentation.slides.items.length}`);
}

const snapshot = await presentation.inspect({
  kind: "deck,slide,textbox,shape,image,table,chart,notes,layout",
  maxChars: 50000,
});
await fs.writeFile(path.join(previewDir, "authored-inspect.ndjson"), snapshot.ndjson);

const candidatePath = path.join(stagingDir, "fcw-7-slide-candidate.pptx");
await (await PresentationFile.exportPptx(presentation)).save(candidatePath);

const requirements = {
  explicitTotalSlideCount: 7,
  requiredNativeTableOwnerSlides: [],
  requiredNativeChartOwnerSlides: [],
};
const fontPolicy = {
  basis: "design",
  families: [LATIN_FONT, JP_FONT],
  scriptFonts: { ea: JP_FONT },
};

await finalizePresentation({
  ...requirements,
  workspaceDir,
  candidatePath,
  finalPath,
  pythonExecutable: runtimePython,
  integrityValidatorPath: path.join(skillDir, "container_tools/inspect_presentation_package_integrity.py"),
  layoutValidatorPath: path.join(skillDir, "container_tools/inspect_presentation_layout_geometry.py"),
  layoutArgs: [
    "--expected-slide-size-emu", "12192000,6858000",
    "--validate-bullet-geometry",
    "--validate-heading-fit",
  ],
  requiredNativeTableOwnerSlides: [],
  fontPolicy,
  verifyArtifactToolImport: true,
  receiptPath: path.join(stagingDir, "FCW_7_slide_examples_2026-09-06_v2.pptx.validation.json"),
});

const finalPresentation = await PresentationFile.importPptx(await FileBlob.load(finalPath));
for (let index = 0; index < finalPresentation.slides.items.length; index += 1) {
  const slide = finalPresentation.slides.getItem(index);
  const preview = await finalPresentation.export({ slide, format: "png", scale: 1 });
  await fs.writeFile(
    path.join(previewDir, `final-slide-${String(index + 1).padStart(2, "0")}.png`),
    new Uint8Array(await preview.arrayBuffer()),
  );
}
const montage = await finalPresentation.export({ format: "png", montage: true, scale: 0.5 });
await fs.writeFile(
  path.join(previewDir, "final-montage.png"),
  new Uint8Array(await montage.arrayBuffer()),
);

const finalSnapshot = await finalPresentation.inspect({
  kind: "deck,slide,textbox,shape,image,table,chart,notes,layout",
  maxChars: 50000,
});
await fs.writeFile(path.join(previewDir, "final-inspect.ndjson"), finalSnapshot.ndjson);

console.log(JSON.stringify({ finalPath, slides: finalPresentation.slides.items.length }));
