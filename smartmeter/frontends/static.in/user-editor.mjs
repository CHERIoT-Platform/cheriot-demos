import {EditorView, basicSetup} from "codemirror"
import {javascript} from "@codemirror/lang-javascript"
import {history, indentWithTab} from "@codemirror/commands"
import {keymap, showPanel} from "@codemirror/view"

var lastSubmittedEditorState;

function panelCtor(view)
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

function panelExt()
{
  return showPanel.of(panelCtor);
}

const initialTextResponse = await fetch("user-policy-initial.txt");
const initialText = await initialTextResponse.text();

const theForm = document.getElementById("theForm");
const meterIdentityInput = document.getElementById("meterIdentity");

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
