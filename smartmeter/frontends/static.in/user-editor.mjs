import {EditorView, basicSetup} from "codemirror"
import {javascript} from "@codemirror/lang-javascript"
import {history, indentWithTab} from "@codemirror/commands"
import {keymap, showPanel} from "@codemirror/view"

function panelCtor(view)
{
  const dom = document.createElement("div");

  const submitButton = document.createElement("button");
  submitButton.type = "submit";
  submitButton.innerHTML = "Compile and run!";
  dom.append(submitButton);

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

function panelExt()
{
  return showPanel.of(panelCtor);
}

const initialTextResponse = await fetch("user-policy-initial.txt");
const initialText = await initialTextResponse.text();

const theForm = document.getElementById("theForm");
const meterIdentityInput = document.getElementById("meterIdentity");

console.log(meterIdentityInput);

let editor = new EditorView({
  extensions: [ basicSetup
              , history()
              , javascript()
              , keymap.of([indentWithTab])
              , panelExt()
              ],
  parent: theForm,
  doc: initialText,
})

theForm.addEventListener("submit", (event) =>
  {
    event.preventDefault();

    const resultField = document.getElementById("theResult");
    const meterIdentity = meterIdentityInput.value;

    fetch(`/postjs/${meterIdentity}`,
          { method: "POST",
            body: editor.state.doc,
            headers: { "Content-Type": "text/plain" }
          })
      .then((response) => {
        if (response.ok)
        {
          theResult.innerHTML = "OK!";
        }
        else
        {
          response.text().then((msg) => { theResult.innerHTML = msg; })
        }})
      .catch((err) => { theResult.innerHTML = error; });

  });
