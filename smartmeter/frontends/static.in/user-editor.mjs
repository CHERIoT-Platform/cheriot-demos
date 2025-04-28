import {EditorView, basicSetup} from "codemirror"
import {javascript} from "@codemirror/lang-javascript"
import {history, indentWithTab} from "@codemirror/commands"
import {keymap, showPanel} from "@codemirror/view"

import * as democonfig from "./democonfig.mjs";

var lastSubmittedEditorState;

const examples =
  [ ["Initial policy", "/static/js-examples/initial.txt" ]
  , ["Hello LEDs", "/static/js-examples/hello-leds.txt" ]
  , ["Attack (zero sensor data)", "/static/js-examples/attack-zero.txt" ]
  , ["Crash", "/static/js-examples/crash.txt" ]
  , ["Attack (pointer forging example)", "/static/js-examples/attack-cgp.txt" ]
  ];

function topPanelCtor(view)
{
  const dom = document.createElement("div");

  const exampleSelector = document.createElement("select");
  exampleSelector.required = false;
  exampleSelector.selectedIndex = -1;

  {
    const opt = document.createElement("option");
    opt.value = -1;
    opt.text = "Load an example script";
    opt.hidden = true;
    opt.disabled = true;
    opt.selected = true;
    exampleSelector.appendChild(opt);
  }

  for (var i = 0; i < examples.length; i++)
  {
    const opt = document.createElement("option");
    opt.value = i;
    opt.text = examples[i][0];
    exampleSelector.appendChild(opt);
  }

  exampleSelector.addEventListener("change", async (event) =>
    {
      const text = await fetch(examples[event.target.selectedOptions[0].value][1]);
      exampleSelector.selectedIndex = 0;
      view.dispatch(
        { changes:
          { insert: await text.text()
          , from: 0
          , to: view.state.doc.length
          }
        });
    }
  );

  dom.append(exampleSelector);

  return { top: true
         , dom
         , update: (update) =>
           {
             if (update.docChanged)
             {
               exampleSelector.selectedIndex = 0;
             }
           }

         };
}

function topPanelExt()
{
  return showPanel.of(topPanelCtor);
}

function bottomPanelCtor(view)
{
  const dom = document.createElement("div");

  const submitButton = document.createElement("button");
  submitButton.type = "submit";
  submitButton.innerHTML = "Compile and run!";
  dom.append(submitButton);

  const revertButton = document.createElement("button");
  revertButton.type = "button";
  revertButton.innerHTML = "Revert to last submitted version";
  revertButton.addEventListener("click", (event) =>
    {
      view.setState(lastSubmittedEditorState);
    }
  );

  dom.append(revertButton);

  const resultField = document.createElement("div");
  resultField.id = "theResult";
  resultField.innerHTML = "";
  dom.append(resultField);

  return { dom
         , update: (update) =>
           {
             if (update.docChanged)
             {
               resultField.innerHTML = "Local changes awaiting submission";
             }
           }
         };
}

function bottomPanelExt()
{
  return showPanel.of(bottomPanelCtor);
}

const initialTextResponse = await fetch("/static/js-examples/initial.txt");
const initialText = await initialTextResponse.text();

const theForm = document.getElementById("theForm");
const meterIdentityInput = document.getElementById("meterIdentity");

if (democonfig.DEFAULT_METER_ID !== null)
{
  meterIdentityInput.value = democonfig.DEFAULT_METER_ID;
}

let editor = new EditorView({
  extensions: [ basicSetup
              , history()
              , javascript()
              , keymap.of([indentWithTab])
              , topPanelExt()
              , bottomPanelExt()
              ],
  parent: theForm,
  doc: initialText,
})

lastSubmittedEditorState = editor.state;

theForm.addEventListener("submit", (event) =>
  {
    event.preventDefault();

    const resultField = document.getElementById("theResult");
    const meterIdentity = meterIdentityInput.value;

    const editorState = editor.state;

    fetch(`/postjs/${meterIdentity}`,
          { method: "POST",
            body: editorState.doc,
            headers: { "Content-Type": "text/plain" }
          })
      .then((response) => {
        if (response.ok)
        {
          theResult.innerHTML = "OK!";
          lastSubmittedEditorState = editorState;
        }
        else
        {
          response.text().then((msg) => { theResult.innerHTML = msg; })
        }})
      .catch((err) => { theResult.innerHTML = error; });

  });
