/* Host stepping fixture: queued messages survive Worker shutdown. */
import { Worker } from "os";
Worker.parent.postMessage(1);
Worker.parent.postMessage(2);
