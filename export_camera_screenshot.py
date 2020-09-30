import cv2

video_name = 0
rectas = {"Entra":[230,343,654,312,663,337],"Sale":[654,212,230,243,663,227]}        #Donde mide
MostrarVideo = False


### Inicio código
vs = cv2.VideoCapture(0)

def ponerLineas(frame,color, rectas):
    for j in rectas:
        i = rectas[j]
        cv2.line(frame,(i[0],i[1]),(i[2],i[3]),color,4)
        cv2.putText(frame, str(j), (i[4],i[5]),
            cv2.FONT_HERSHEY_SIMPLEX, 0.65, color, 1)
    return frame

T = 0
while True:
    # grab the next frame from the video file
    (grabbed, frame) = vs.read()
    T=T+1
    if T>25: #espera aproximadamente un segundo antes de tomar la "foto"
        frame = ponerLineas(frame,(0,255,255), rectas)
        try:
            cv2.imwrite("screenshot.jpg", frame)
            print("Exportación ok")
        except:
            print("Error en la exportación")
        break
    #En caso de que se quiera mostrar el video
    if MostrarVideo:
        key = cv2.waitKey(1) & 0xFF
        if key == ord("q"):
            break
        cv2.imshow("Frame", frame)
