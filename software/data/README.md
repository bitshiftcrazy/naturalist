# The Naturalist Data

This directory contains the mushrooms, trait vocabulary and keyboard shortcuts used for the Naturalist.

The current version contains **25 mushroom species**, **24 trait categories** with **118 values**, and **45 shortcuts**.

- `mushrooms.json` species, names, traits, images, illustration provenance and sources for mushroom characteristics
- `trait_vocabulary.json` the available categories and values, their German and English labels, and notes
- `shortcuts.json` keyboard keys mapped to individual trait values

Information sources for individual mushrooms are listed in their `trait_sources` arrays, general sources for the vocabulary are recorded in `sources_for_vocabulary`. The sources for the pictures are also in the original's EXIFs (but they misattribute Anna Maria Hussey's illustrations to her husband, which is annoying).

## Adding Your Own Mushroom Data

### Adding a species

Add an object to the `mushrooms` array in `mushrooms.json`. Here is a **shortened example** based on the existing Wiesen-Champignon/Field Mushroom entry (the real record has more traits):

    {
      "id": "agaricus_campestris",
      "scientific_name": "Agaricus campestris",
      "common_names_de": [
        "Wiesen-Champignon"
      ],
      "common_names_en": [
        "Field Mushroom"
      ],
      "image": "assets/mushrooms/agaricus_campestris.png",
      "illustration": {
        "publication_year": 1896,
        "artists": [
          "Schmalfuß, Albin"
        ],
        "book_authors": [
          "Michael, Edmund"
        ],
        "sources": {
          "flickr": "https://www.flickr.com/photos/biodivlibrary/8509421351/",
          "bhl": "https://www.biodiversitylibrary.org/page/1266201#page/153/mode/1up"
        },
        "rights_statements": {
          "flickr": "Public Domain",
          "bhl": "Public Domain"
        }
      },
      "traits": {
        "body_form": [
          "cap_stem"
        ],
        "fertile_surface": [
          "gills"
        ],
        "habitat": [
          "grassland",
          "park_garden"
        ]
      },
      "trait_sources": [
        "https://www.first-nature.com/fungi/agaricus-campestris.php"
      ]
    }

Each species needs:
- a unique lowercase `id`, a scientific name, and German and English common names
- an `image` path in the form `assets/mushrooms/<id>.png`, relative to `software/`
- an `illustration` object with the publication year, artists, book authors, source links and rights statements
- a `traits` object whose keys are facet IDs from `trait_vocabulary.json` and whose values are arrays of valid trait IDs, use `null` where a trait does not exist
- a `trait_sources` array citing the sources used to assign the mushroom characteristics

You don't need to fill in every trait, but ideally you stick to the existing trait vocabulary. Also note that a trait not listed is not the same as a trait explicitly set as absent.

### Adding traits

The vocabulary is in the `facets` array of `trait_vocabulary.json`. 
Each facet has an ID, German and English labels, a `tier` (`primary` or `secondary`), a `cardinality` (`one` or `many`), as well as a list of possible values.
Some also contain notes or explanations.

For example:

    {
      "id": "ring_presence",
      "label_de": "Ring am Stiel",
      "label_en": "Ring on stem",
      "tier": "primary",
      "cardinality": "one",
      "values": [
        {
          "id": "present",
          "label_de": "Ring erkennbar",
          "label_en": "Ring visible"
        },
        {
          "id": "absent",
          "label_de": "Kein Ring erkennbar",
          "label_en": "No ring visible"
        }
      ],
      "note": "Die Beobachtung «kein Ring erkennbar» ist kein sicherer Nachweis, dass es keinen Ring gibt.",
      "note_en": "Not seeing a ring is not proof that no ring is present."
    }


To add a trait, you can add a value to an existing facet or define a new facet.
Make sure to use unique IDs, provide both language labels and update the species that uses the new values.

### Adding shortcuts

Shortcuts are objects in the `bindings` array of `shortcuts.json`. 

An existing example:

    {
      "key": "Q",
      "facet": "staining",
      "value": "blue",
      "label_de": "Quetschung blaut",
      "label_en": "Bruises blue"
    }


Each binding has a unique `key`, a `facet` ID, a valid `value` ID in that facet, and German and English labels. 
Check for existing key combinations (and conflicts!) before adding a new one, also keep the Naturalist keyboard's weird layout in mind.

**If you want to send me a PR, please include the sources for your additions.** 

## Adding Mushroom Pictures for New Species

The historical illustrations are from the [Biodiversity Heritage Library](https://www.biodiversitylibrary.org/) and its Flickr collection. 

In the current archive:

- `software/assets/originals/` contains **26 original JPG illustrations**
- `software/assets/mushrooms/` contains **25 prepared PNG illustrations**

To prepare an illustration for use on the eInk, remove its background and crop or resize it. 
The current PNGs have transparent backgrounds and fit within **320 × 320 pixels**. 
Save the result as `<latin_name>.png` in `software/assets/mushrooms/` and save the source image in `software/assets/originals/`. If you download from the Biodiversity Heritage Library flickr, take the largest size available, sometimes a plate contains several mushrooms and you want enough pixels left after cropping.

## Sources Overview

This table lists the **25 species currently included in `mushrooms.json`**.

- **PD** = `Public Domain`
- **NIC** = `NOT_IN_COPYRIGHT`

This is what Biodiverstity Heritage Library lists, I did not ask a copyright lawyer...

<table>
  <thead>
    <tr>
      <th>Species</th>
      <th>Illustration sources / rights</th>
      <th>Year</th>
      <th>Credits</th>
    </tr>
  </thead>
  <tbody>
    <tr>
      <td>1. Fliegenpilz<br><em>Amanita muscaria</em></td>
      <td><a href="https://www.flickr.com/photos/biodivlibrary/6358013625/in/album-72157628037086347">Flickr</a> (PD)<br><a href="https://www.biodiversitylibrary.org/page/3270680#page/95/mode/1up">BHL</a> (NIC)</td>
      <td>1891</td>
      <td>Artist: not attributed<br>Author: Dufour, Léon,</td>
    </tr>
    <tr>
      <td>2. Grüner Knollenblätterpilz<br><em>Amanita phalloides</em></td>
      <td><a href="https://www.flickr.com/photos/biodivlibrary/6358013781/in/album-72157628037086347">Flickr</a> (PD)<br><a href="https://www.biodiversitylibrary.org/page/3270805#page/97/mode/1up">BHL</a> (NIC)</td>
      <td>1891</td>
      <td>Artist: not attributed<br>Author: Dufour, Léon</td>
    </tr>
    <tr>
      <td>3. Pantherpilz<br><em>Amanita pantherina</em></td>
      <td><a href="https://www.flickr.com/photos/biodivlibrary/6358013975/in/album-72157628037086347">Flickr</a> (PD)<br><a href="https://www.biodiversitylibrary.org/page/3270807#page/99/mode/1up">BHL</a> (NIC)</td>
      <td>1891</td>
      <td>Artist: not attributed<br>Author: Dufour, Léon</td>
    </tr>
    <tr>
      <td>4. Perlpilz<br><em>Amanita rubescens</em></td>
      <td><a href="https://www.flickr.com/photos/biodivlibrary/6358013975/in/album-72157628037086347">Flickr</a> (PD)<br><a href="https://www.biodiversitylibrary.org/page/3270807#page/99/mode/1up">BHL</a> (NIC)</td>
      <td>1891</td>
      <td>Artist: not attributed<br>Author: Dufour, Léon</td>
    </tr>
    <tr>
      <td>5. Steinpilz<br><em>Boletus edulis</em></td>
      <td><a href="https://www.flickr.com/photos/biodivlibrary/6358026439/in/album-72157628037086347">Flickr</a> (PD)<br><a href="https://www.biodiversitylibrary.org/page/3270845#page/199/mode/1up">BHL</a> (NIC)</td>
      <td>1891</td>
      <td>Artist: not attributed<br>Author: Dufour, Léon</td>
    </tr>
    <tr>
      <td>6. Maronenröhrling<br><em>Imleria badia</em></td>
      <td><a href="https://www.flickr.com/photos/biodivlibrary/8509403713/in/album-72157632865665228">Flickr</a> (PD)<br><a href="https://www.biodiversitylibrary.org/page/1266092#page/71/mode/1up">BHL</a> (PD)</td>
      <td>1896</td>
      <td>Artist: Schmalfuß, Albin<br>Author: Michael, Edmund</td>
    </tr>
    <tr>
      <td>7. Gemeiner Rotfußröhrling<br><em>Xerocomellus chrysenteron</em></td>
      <td><a href="https://www.flickr.com/photos/biodivlibrary/8510511800/in/album-72157632865665228">Flickr</a> (PD)<br><a href="https://www.biodiversitylibrary.org/page/1266091#page/69/mode/1up">BHL</a> (PD)</td>
      <td>1896</td>
      <td>Artist: Schmalfuß, Albin<br>Author: Michael, Edmund</td>
    </tr>
    <tr>
      <td>8. Butterpilz<br><em>Suillus luteus</em></td>
      <td><a href="https://www.flickr.com/photos/biodivlibrary/8509401299/in/album-72157632865665228">Flickr</a> (PD)<br><a href="https://www.biodiversitylibrary.org/page/1266131#page/59/mode/1up">BHL</a> (PD)</td>
      <td>1896</td>
      <td>Artist: Schmalfuß, Albin<br>Author: Michael, Edmund</td>
    </tr>
    <tr>
      <td>9. Gemeiner Birkenpilz<br><em>Leccinum scabrum</em></td>
      <td><a href="https://www.flickr.com/photos/biodivlibrary/8510509164/in/album-72157632865665228">Flickr</a> (PD)<br><a href="https://www.biodiversitylibrary.org/page/1266127#page/55/mode/1up">BHL</a> (PD)</td>
      <td>1896</td>
      <td>Artist: Schmalfuß, Albin<br>Author: Michael, Edmund</td>
    </tr>
    <tr>
      <td>10. Gallenröhrling<br><em>Tylopilus felleus</em></td>
      <td><a href="https://www.flickr.com/photos/biodivlibrary/8509400359/in/album-72157632865665228">Flickr</a> (PD)<br><a href="https://www.biodiversitylibrary.org/page/1266126#page/53/mode/1up">BHL</a> (PD)</td>
      <td>1896</td>
      <td>Artist: Schmalfuß, Albin<br>Author: Michael, Edmund</td>
    </tr>
    <tr>
      <td>11. Echter Pfifferling<br><em>Cantharellus cibarius</em></td>
      <td><a href="https://www.flickr.com/photos/biodivlibrary/8510517024/in/album-72157632865665228">Flickr</a> (PD)<br><a href="https://www.biodiversitylibrary.org/page/1266154#page/95/mode/1up">BHL</a> (PD)</td>
      <td>1896</td>
      <td>Artist: Schmalfuß, Albin<br>Author: Michael, Edmund</td>
    </tr>
    <tr>
      <td>12. Semmelstoppelpilz<br><em>Hydnum repandum</em></td>
      <td><a href="https://www.flickr.com/photos/biodivlibrary/8510515094/in/album-72157632865665228">Flickr</a> (PD)<br><a href="https://www.biodiversitylibrary.org/page/1266148#page/85/mode/1up">BHL</a> (PD)</td>
      <td>1896</td>
      <td>Artist: Schmalfuß, Albin<br>Author: Michael, Edmund</td>
    </tr>
    <tr>
      <td>13. Parasol / Riesenschirmling<br><em>Macrolepiota procera</em></td>
      <td><a href="https://www.flickr.com/photos/biodivlibrary/8509418243/in/album-72157632865665228">Flickr</a> (PD)<br><a href="https://www.biodiversitylibrary.org/page/1266181#page/141/mode/1up">BHL</a> (PD)</td>
      <td>1896</td>
      <td>Artist: Schmalfuß, Albin<br>Author: Michael, Edmund</td>
    </tr>
    <tr>
      <td>14. Schopftintling<br><em>Coprinus comatus</em></td>
      <td><a href="https://www.flickr.com/photos/biodivlibrary/8510518070/in/album-72157632865665228">Flickr</a> (PD)<br><a href="https://www.biodiversitylibrary.org/page/1266105#page/101/mode/1up">BHL</a> (PD)</td>
      <td>1896</td>
      <td>Artist: Schmalfuß, Albin<br>Author: Michael, Edmund</td>
    </tr>
    <tr>
      <td>15. Glimmertintling<br><em>Coprinellus micaceus</em></td>
      <td><a href="https://www.flickr.com/photos/biodivlibrary/8619423678/">Flickr</a> (PD)<br><a href="https://www.biodiversitylibrary.org/page/2976696#page/246/mode/1up">BHL</a> (NIC)</td>
      <td>1847</td>
      <td>Artist: Hussey, Anna Maria; Reed, Frances<br>Author: Hussey, Anna Maria; Reed, Frances</td>
    </tr>
    <tr>
      <td>16. Rötlicher Lacktrichterling<br><em>Laccaria laccata</em></td>
      <td><a href="https://www.flickr.com/photos/biodivlibrary/8619421490/">Flickr</a> (PD)<br><a href="https://www.biodiversitylibrary.org/page/2976664#page/214/mode/1up">BHL</a> (NIC)</td>
      <td>1847</td>
      <td>Artist: Hussey, Anna Maria; Reed, Frances<br>Author: Hussey, Anna Maria; Reed, Frances</td>
    </tr>
    <tr>
      <td>17. Grünblättriger Schwefelkopf<br><em>Hypholoma fasciculare</em></td>
      <td><a href="https://www.flickr.com/photos/biodivlibrary/6358023637/">Flickr</a> (PD)<br><a href="https://www.biodiversitylibrary.org/page/3270838#page/179/mode/1up">BHL</a> (NIC)</td>
      <td>1891</td>
      <td>Artist: not attributed<br>Author: Dufour, Léon</td>
    </tr>
    <tr>
      <td>18. Austernseitling<br><em>Pleurotus ostreatus</em></td>
      <td><a href="https://www.flickr.com/photos/biodivlibrary/6358016979/">Flickr</a> (PD)<br><a href="https://www.biodiversitylibrary.org/page/3270618#page/125/mode/1up">BHL</a> (NIC)</td>
      <td>1891</td>
      <td>Artist: not attributed<br>Author: Dufour, Léon</td>
    </tr>
    <tr>
      <td>19. Nebelkappe<br><em>Clitocybe nebularis</em></td>
      <td><a href="https://www.flickr.com/photos/biodivlibrary/6358016117/">Flickr</a> (PD)<br><a href="https://www.biodiversitylibrary.org/page/3270719#page/119/mode/1up">BHL</a> (NIC)</td>
      <td>1891</td>
      <td>Artist: not attributed<br>Author: Dufour, Léon</td>
    </tr>
    <tr>
      <td>20. Waldfreund-Rübling<br><em>Gymnopus dryophilus</em></td>
      <td><a href="https://www.flickr.com/photos/biodivlibrary/8618312369/">Flickr</a> (PD)<br><a href="https://www.biodiversitylibrary.org/page/2976630#page/180/mode/1up">BHL</a> (NIC)</td>
      <td>1847</td>
      <td>Artist: Hussey, Anna Maria<br>Author: Hussey, Anna Maria; Reed, Frances</td>
    </tr>
    <tr>
      <td>21. Wiesen-Champignon<br><em>Agaricus campestris</em></td>
      <td><a href="https://www.flickr.com/photos/biodivlibrary/8509421351/">Flickr</a> (PD)<br><a href="https://www.biodiversitylibrary.org/page/1266201#page/153/mode/1up">BHL</a> (PD)</td>
      <td>1896</td>
      <td>Artist: Schmalfuß, Albin<br>Author: Michael, Edmund</td>
    </tr>
    <tr>
      <td>22. Speisemorchel<br><em>Morchella esculenta</em></td>
      <td><a href="https://www.flickr.com/photos/biodivlibrary/6358032311/">Flickr</a> (PD)<br><a href="https://www.biodiversitylibrary.org/page/3270667">BHL</a> (NIC)</td>
      <td>1891</td>
      <td>Artist: not attributed<br>Author: Dufour, Léon</td>
    </tr>
    <tr>
      <td>23. Krause Glucke / Fette Henne<br><em>Sparassis crispa</em></td>
      <td><a href="https://www.flickr.com/photos/biodivlibrary/8509407829/">Flickr</a> (PD)<br><a href="https://www.biodiversitylibrary.org/page/1266099">BHL</a> (PD)</td>
      <td>1896</td>
      <td>Artist: Schmalfuß, Albin<br>Author: Michael, Edmund</td>
    </tr>
    <tr>
      <td>24. Flaschenstäubling<br><em>Lycoperdon perlatum</em></td>
      <td><a href="https://www.flickr.com/photos/biodivlibrary/6358031159/">Flickr</a> (PD)<br><a href="https://www.biodiversitylibrary.org/page/3270622">BHL</a> (NIC)</td>
      <td>1891</td>
      <td>Artist: not attributed<br>Author: Dufour, Léon</td>
    </tr>
    <tr>
      <td>25. Schwefelporling<br><em>Laetiporus sulphureus</em></td>
      <td><a href="https://www.flickr.com/photos/biodivlibrary/6358024981/">Flickr</a> (PD)<br><a href="https://www.biodiversitylibrary.org/page/3270844">BHL</a> (NIC)</td>
      <td>1891</td>
      <td>Artist: not attributed<br>Author: Dufour, Léon</td>
    </tr>
  </tbody>
</table>


If you'd like to contribute an illustration, please:

- include the original image and literature links, source details, and any available artist/author attribution
- check that it is actually out of copyright or in the public domain, a missing or ambiguous rights statement won't work
- do a little research on the illustrator and publisher: history is messy, and sure, I can't guarantee that nobody painting mushrooms 150 years ago was an asshole, but I'd at least like to exclude the obvious, historically documented assholes (Nazis, racists, etc.)
