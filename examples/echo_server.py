import socket
import logging
import sys

logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(levelname)s - %(message)s',
    datefmt='%Y-%m-%d %H:%M:%S',
    stream=sys.stdout
)

logger = logging.getLogger(__name__)

def run_echo_server(host='127.0.0.1', port=8888):
    server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)

    try:
        server_socket.bind((host, port))
        server_socket.listen(5)

        logger.info(f"Echo server started on {host}:{port}")
        logger.info("Waiting for connections...")

        while True:
            client_socket, client_address = server_socket.accept()
            logger.info(f"Client connected: {client_address[0]}:{client_address[1]}")

            try:
                while True:
                    data = client_socket.recv(1024)

                    if not data:
                        logger.info(f"Client disconnected: {client_address[0]}:{client_address[1]}")
                        break

                    message = data.decode('utf-8').strip()
                    logger.info(f"Received from {client_address[0]}:{client_address[1]}: {message} ({len(data)} bytes)")

                    client_socket.send(data)
                    logger.debug(f"Echo sent back: {message}")

            except ConnectionResetError:
                logger.warning(f"Connection reset by client: {client_address[0]}:{client_address[1]}")
            except Exception as e:
                logger.error(f"Error handling client {client_address}: {e}")
            finally:
                client_socket.close()
                logger.info(f"Connection closed: {client_address[0]}:{client_address[1]}")

    except KeyboardInterrupt:
        logger.info("Server stopped by user")
    except Exception as e:
        logger.error(f"Fatal error: {e}")
    finally:
        server_socket.close()
        logger.info("Server shutdown complete")

if __name__ == "__main__":
    run_echo_server()
