#!/usr/bin/env node
/** Build editable SVG publication layouts from actual Blender/TexUtil outputs. */
const fs = require('node:fs/promises');
const path = require('node:path');
const crypto = require('node:crypto');
const sharp = require('sharp');
const root = path.resolve(__dirname, '..');
const out = path.resolve(root, process.argv[2] || 'out/publication');
const sources = new Map();
const ink = '#f4f7ff', muted = '#b7c9dc', navy = '#071621';
const green = '#54dda9', blue = '#5ab9ff', gold = '#ffbc62', purple = '#bb91ff';
const esc = value => String(value).replaceAll('&', '&amp;').replaceAll('<', '&lt;').replaceAll('>', '&gt;').replaceAll('"', '&quot;');
let parts = [];
function text(x, y, value, size = 24, color = ink, weight = 400, extra = '') {
    parts.push(`<text x="${x}" y="${y}" font-size="${size}" fill="${color}" font-weight="${weight}" ${extra}>${esc(value)}</text>`);
}
function rect(x, y, width, height, fill, stroke = 'none', radius = 0, extra = '') {
    parts.push(`<rect x="${x}" y="${y}" width="${width}" height="${height}" rx="${radius}" fill="${fill}" stroke="${stroke}" ${extra}/>`);
}
function line(x1, y1, x2, y2, color, width = 2, extra = '') {
    parts.push(`<path d="M${x1} ${y1} L${x2} ${y2}" fill="none" stroke="${color}" stroke-width="${width}" ${extra}/>`);
}
function begin(width, height) {
    parts = [`<svg xmlns="http://www.w3.org/2000/svg" xmlns:xlink="http://www.w3.org/1999/xlink" width="${width}" height="${height}" viewBox="0 0 ${width} ${height}" font-family="Arial, Helvetica, sans-serif"><defs><radialGradient id="bg" cx="60%" cy="40%" r="85%"><stop stop-color="#102b3a"/><stop offset="1" stop-color="#040d16"/></radialGradient><linearGradient id="panel" x2="0" y2="1"><stop stop-color="#102633" stop-opacity=".8"/><stop offset="1" stop-color="#07141f" stop-opacity=".9"/></linearGradient><linearGradient id="badge" x2="1" y2="1"><stop stop-color="white" stop-opacity=".35"/><stop offset="1" stop-color="white" stop-opacity="0"/></linearGradient></defs>`];
    rect(0, 0, width, height, 'url(#bg)');
}
function brand(x, y, size = 76) {
    text(x, y, 'Foliage', size, '#ffffff', 700);
    text(x + size * 3.56, y - size * .36, 'Util', size * .30, '#ffffff', 700);
}
function header(title, subtitle) {
    brand(30, 82, 80);
    text(415, 51, title, 41, ink, 700);
    text(417, 88, subtitle, 24, muted);
}
function panel(x, y, width, height, number, title, subtitle, color, titleSize = 31) {
    rect(x, y, width, height, 'url(#panel)', color, 15, 'stroke-width="1.5" stroke-opacity=".8"');
    parts.push(`<circle cx="${x + 47}" cy="${y + 45}" r="28" fill="${color}"/><circle cx="${x + 47}" cy="${y + 45}" r="28" fill="url(#badge)"/>`);
    text(x + 47, y + 55, number, 29, navy, 700, 'text-anchor="middle"');
    text(x + 91, y + 42, title, titleSize, ink, 700);
    if (subtitle) text(x + 91, y + 73, subtitle, 21, muted);
}
async function asset(relative) {
    const absolute = path.resolve(root, relative);
    if (sources.has(absolute)) return sources.get(absolute);
    const bytes = await fs.readFile(absolute);
    const {data, info} = await sharp(bytes).ensureAlpha().raw().toBuffer({resolveWithObject: true});
    let left = info.width, top = info.height, right = 0, bottom = 0;
    for (let y = 0; y < info.height; y++) for (let x = 0; x < info.width; x++) {
        if (data[(y * info.width + x) * 4 + 3] > 10) {
            left = Math.min(left, x); right = Math.max(right, x);
            top = Math.min(top, y); bottom = Math.max(bottom, y);
        }
    }
    if (left > right) throw new Error(`Empty alpha layer: ${absolute}`);
    const a = {relative, width: info.width, height: info.height, bounds: [left, top, right - left + 1, bottom - top + 1], uri: `data:image/png;base64,${bytes.toString('base64')}`, sha256: crypto.createHash('sha256').update(bytes).digest('hex')};
    sources.set(absolute, a);
    return a;
}
async function picture(relative, x, y, width, height, crop = null, align = 'xMidYMid meet') {
    const a = await asset(relative);
    const bounds = crop || a.bounds;
    parts.push(`<svg x="${x}" y="${y}" width="${width}" height="${height}" viewBox="${bounds.join(' ')}" preserveAspectRatio="${align}" overflow="hidden"><image width="${a.width}" height="${a.height}" xlink:href="${a.uri}"/></svg>`);
}
async function layer(name, x, y, width, height, crop = null, align = 'xMidYMid meet') {
    await picture(`out/publication/layers/${name}.png`, x, y, width, height, crop, align);
}
function arrow(x1, y, x2, color) {
    line(x1, y, x2 - 9, y, color, 3);
    parts.push(`<path d="M${x2 - 11} ${y - 7} L${x2} ${y} L${x2 - 11} ${y + 7} Z" fill="${color}"/>`);
}
async function save(name, width, height) {
    parts.push('</svg>');
    const svg = parts.join('\n');
    const filename = path.join(out, name);
    await fs.writeFile(filename + '.svg', svg);
    await sharp(Buffer.from(svg)).resize(width, height).png({compressionLevel: 9}).toFile(filename + '.png');
    if (name === 'foliageutil-social-preview') await sharp(Buffer.from(svg)).resize(width, height).jpeg({quality: 94, chromaSubsampling: '4:4:4'}).toFile(filename + '.jpg');
    console.log(`Built ${name}: ${width} x ${height}`);
}
async function workflow() {
    begin(1536, 1024);
    header('Procedural foliage workflow', 'Seeded JSON graphs  •  Reusable parts  •  Textured meshes');
    panel(24, 119, 736, 390, '01', 'Build the structure', 'Shape trunks, roots and branching paths.', green);
    const nodes = [['trunk', 55, 242], ['branch', 260, 242], ['scatter', 465, 242], ['tube', 55, 334], ['leaf / card', 260, 334], ['instance', 465, 334]];
    nodes.forEach(([label, x, y]) => { rect(x, y, 165, 52, '#142e3c', green, 8, 'stroke-opacity=".55"'); text(x + 82.5, y + 34, label, 25, ink, 700, 'text-anchor="middle"'); });
    arrow(222, 268, 255, green); arrow(427, 268, 460, green);
    line(136, 295, 136, 330, green); line(548, 295, 548, 330, green); arrow(426, 360, 460, green);
    text(57, 432, 'Curvature + noise profiles', 23, ink, 700);
    text(57, 466, 'Root flare  •  Solid wood unions  •  Seed control', 21, muted);
    panel(780, 119, 732, 390, '02', 'Grow in stages', 'Save when each part appears and develops.', blue);
    await layer('growth', 802, 218, 686, 213);
    const growthLabels = [['Trunk', 879], ['Branches', 1052], ['Leaves', 1221], ['Mature', 1396]];
    growthLabels.forEach(([label, x]) => text(x, 462, label, 21, ink, 700, 'text-anchor="middle"'));
    panel(24, 529, 736, 416, '03', 'Add texture + variation', 'TexUtil spritesheets mapped onto curved cards.', gold);
    const atlas = 'out/samples/assets/sakura/sprigs.atlas.assets/color.png';
    rect(53, 641, 278, 231, '#19303d', '#3a4d5c', 7);
    await picture(atlas, 57, 645, 270, 223, [0, 0, 1152, 960]);
    for (let col = 1; col < 3; col++) line(53 + col * 278 / 3, 641, 53 + col * 278 / 3, 872, '#567386', 1);
    line(53, 756.5, 331, 756.5, '#567386', 1);
    arrow(349, 749, 390, gold);
    await layer('sakura', 408, 644, 295, 230);
    text(56, 910, 'RGBA atlas', 21, gold, 700);
    text(409, 910, 'Random cell + size + orientation', 19, muted);
    panel(780, 529, 732, 416, '04', 'Export with fewer triangles', 'Configured LODs or fitted, texture-baked cards.', purple);
    await layer('willow-bake', 800, 638, 686, 220);
    ['Original', 'CardBake', 'CardBake low'].forEach((value, i) => text(914 + i * 229, 881, value, 19, ink, 700, 'text-anchor="middle"'));
    ['810,482', '52,082', '44,882'].forEach((value, i) => text(914 + i * 229, 906, value + ' tris', 19, i ? purple : muted, 700, 'text-anchor="middle"'));
    text(804, 935, 'Willow example: up to 94.5% fewer triangles', 21, ink, 700);
    text(31, 991, 'CPU generation + baking', 22, green, 700);
    text(476, 991, 'GLB / OBJ  •  PBR textures  •  Wind metadata', 21, muted);
    text(1505, 991, 'Actual exported meshes', 21, muted, 400, 'text-anchor="end"');
    await save('foliageutil-workflow', 1536, 1024);
}
async function showcase() {
    begin(1536, 1024);
    header('One system. Many kinds of plants.', 'Composable graphs  •  Native TexUtil materials  •  Blender renders');
    panel(24, 119, 736, 530, '01', 'Sakura', 'Flowering sprig atlases on cascading branches.', green);
    await layer('sakura', 47, 224, 688, 373);
    text(55, 624, 'Rooted wood  +  seeded blossom clusters', 22, green, 700);
    panel(780, 119, 732, 530, '02', 'Weeping willow', 'Long hanging strands, dense foliage and hefty bark.', blue);
    await layer('willow', 804, 222, 684, 374);
    text(812, 624, 'Detailed geometry  +  packed cards  +  CardBake', 21, blue, 700);
    const xs = [24, 400, 776, 1152];
    [['Rose', 'Curved, veined petals', gold], ['Sunflower', 'Petals + facing seed scatter', purple], ['Birch', 'Branching + bark textures', gold], ['Bamboo + wheat', 'Stems, leaves and grains', green]].forEach(([title, subtitle, color], i) => {
        rect(xs[i], 669, 360, 293, 'url(#panel)', color, 12, 'stroke-width="1.5"');
        text(xs[i] + 19, 704, title, 27, ink, 700);
        text(xs[i] + 19, 734, subtitle, 18, muted);
    });
    await layer('rose-detail', 38, 750, 331, 195);
    await layer('sunflower-detail', 417, 750, 326, 195);
    await layer('birch', 789, 750, 332, 195);
    await layer('bamboo', 1160, 750, 185, 197);
    await layer('wheat', 1310, 750, 188, 197);
    line(34, 995, 308, 995, '#577388', 1);
    text(768, 1002, 'Working examples, not a fixed species list. Build your own.', 23, muted, 400, 'text-anchor="middle"');
    line(1231, 995, 1502, 995, '#577388', 1);
    await save('foliageutil-samples', 1536, 1024);
}
async function social() {
    begin(1280, 640);
    await layer('willow', -240, 24, 652, 743, null, 'xMidYMin meet');
    await layer('birch', 824, 97, 528, 693, null, 'xMidYMin meet');
    await layer('bamboo', 1100, 51, 374, 694, null, 'xMidYMin meet');
    await layer('wheat', 1072, 276, 350, 556, null, 'xMidYMin meet');
    await layer('rose', 954, 446, 215, 390, null, 'xMidYMin meet');
    text(410, 131, 'PROCEDURAL FOLIAGE', 18, green, 700, 'letter-spacing="1.4"');
    brand(402, 267, 87);
    text(410, 341, 'Seeded foliage', 37, ink, 700);
    text(410, 389, 'from JSON graphs', 37, ink, 700);
    text(411, 504, 'Trees  •  Plants  •  Flowers', 22, muted);
    text(411, 546, 'Growth  •  LODs  •  CardBake', 19, green, 700);
    text(411, 580, 'GLB + OBJ  •  TexUtil materials', 18, muted);
    await save('foliageutil-social-preview', 1280, 640);
}
async function main() {
    await fs.mkdir(out, {recursive: true});
    await workflow(); await showcase(); await social();
    const manifest = {renderer: `SVG + sharp ${sharp.versions.sharp}`, font: 'Arial / Helvetica / sans-serif', note: 'Actual Blender render layers and native TexUtil maps embedded without repainting. Presentation scales vary between species; willow triangle comparison retains its matched source scale.', outputs: ['foliageutil-workflow', 'foliageutil-samples', 'foliageutil-social-preview'], sources: [...sources.values()].map(({uri, ...value}) => value)};
    await fs.writeFile(path.join(out, 'manifest.json'), JSON.stringify(manifest, null, 2) + '\n');
}
main().catch(error => { console.error(error); process.exitCode = 1; });
