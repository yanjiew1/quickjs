/* Worker for the Windows native wait handle overflow regression. */
import * as os from "os";

const parent = os.Worker.parent;
parent.onmessage = ({ data }) => {
    if (data === "ping")
        parent.postMessage("pong");
    else if (data === "stop")
        parent.onmessage = null;
    else
        throw Error("unexpected parent message");
};
parent.postMessage("ready");
