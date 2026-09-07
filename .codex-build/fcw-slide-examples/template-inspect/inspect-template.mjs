import fs from "node:fs/promises";
import path from "node:path";
import { FileBlob, PresentationFile } from "@oai/artifact-tool";

const sourcePath = "/Users/saitougenbu/.codex/plugins/cache/openai-curated-remote/openai-templates/0.1.1/skills/artifact-template-project-kickoff/assets/reference.pptx";
const outputDir = path.dirname(new URL(import.meta.url).pathname);

const presentation = await PresentationFile.importPptx(await FileBlob.load(sourcePath));
const snapshot = await presentation.inspect({
  kind: "deck,slide,textbox,shape,image,table,chart,notes,layout",
  maxChars: 50000,
});
await fs.writeFile(path.join(outputDir, "template-inspect.ndjson"), snapshot.ndjson);

for (let index = 0; index < presentation.slides.items.length; index += 1) {
  const slide = presentation.slides.getItem(index);
  const preview = await presentation.export({ slide, format: "png", scale: 1 });
  await fs.writeFile(
    path.join(outputDir, `template-slide-${String(index + 1).padStart(2, "0")}.png`),
    new Uint8Array(await preview.arrayBuffer()),
  );
}

const montage = await presentation.export({ format: "png", montage: true, scale: 0.5 });
await fs.writeFile(
  path.join(outputDir, "template-montage.png"),
  new Uint8Array(await montage.arrayBuffer()),
);

console.log(JSON.stringify({
  slides: presentation.slides.items.length,
  masters: presentation.masters.items.length,
  layouts: presentation.layouts.items.length,
}));
